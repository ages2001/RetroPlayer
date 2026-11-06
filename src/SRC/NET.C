/* Winsock 1.1 adapters for the synchronous interfaces used by VLC.
 * IPv6 and overlapped I/O require a newer network provider and return explicit
 * unsupported errors here. IPv4 TCP/UDP and scatter/gather I/O are implemented.
 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <limits.h>

#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0x00000008
#endif
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0x00000400
#endif
/* Winsock reports a too-small getnameinfo output buffer as WSAEFAULT. */
#ifndef EAI_OVERFLOW
# define EAI_OVERFLOW WSAEFAULT
#endif

int WSAAPI rp_ioctl(SOCKET socket,DWORD code,void *input,DWORD in_size,
                   void *output,DWORD out_size,DWORD *bytes,
                   OVERLAPPED *overlapped,LPWSAOVERLAPPED_COMPLETION_ROUTINE completion)
{
    u_long value;
    int result;
    if (overlapped || completion) { WSASetLastError(WSAEOPNOTSUPP); return SOCKET_ERROR; }
    if (!bytes) { WSASetLastError(WSAEFAULT); return SOCKET_ERROR; }
    *bytes=0;
    if (code==FIONBIO) {
        if (!input || in_size<sizeof(value)) { WSASetLastError(WSAEFAULT); return SOCKET_ERROR; }
        value=*(u_long *)input;
        return ioctlsocket(socket,(long)code,&value);
    }
    if (code==FIONREAD || code==SIOCATMARK) {
        if (!output || out_size<sizeof(value)) { WSASetLastError(WSAEFAULT); return SOCKET_ERROR; }
        result=ioctlsocket(socket,(long)code,&value);
        if (!result) { *(u_long *)output=value; *bytes=sizeof(value); }
        return result;
    }
    WSASetLastError(WSAEOPNOTSUPP); return SOCKET_ERROR;
}

static void *allocate(SIZE_T size) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size); }
static void release(void *value) { if (value) HeapFree(GetProcessHeap(), 0, value); }
static unsigned length(const char *text) { unsigned n=0; while (text[n]) ++n; return n; }
static void copy(void *out, const void *in, unsigned size)
{
    unsigned char *dest=out;
    const unsigned char *src=in;
    while (size--) *dest++=*src++;
}
static char *duplicate(const char *text)
{
    unsigned size=length(text)+1;
    char *result=allocate(size);
    if (result) copy(result,text,size);
    return result;
}
static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static char *decimal(char *out, unsigned value)
{
    char digits[10]; unsigned used=0;
    do { digits[used++]=(char)('0'+value%10); value/=10; } while (value);
    while (used) *out++=digits[--used];
    *out=0; return out;
}
static int dns_error(int error)
{
    if (error == WSATRY_AGAIN) return EAI_AGAIN;
    if (error == WSAHOST_NOT_FOUND || error == WSANO_DATA) return EAI_NONAME;
    if (error == WSAENOBUFS) return EAI_MEMORY;
    return EAI_FAIL;
}

void WSAAPI rp_freeaddrinfo(struct addrinfo *list)
{
    while (list) {
        struct addrinfo *next=list->ai_next;
        release(list->ai_addr); release(list->ai_canonname); release(list);
        list=next;
    }
}

int WSAAPI rp_getaddrinfo(const char *node, const char *service,
                        const struct addrinfo *hints, struct addrinfo **output)
{
    int flags=hints ? hints->ai_flags : 0;
    int family=hints ? hints->ai_family : AF_UNSPEC;
    int type=hints ? hints->ai_socktype : 0;
    int protocol=hints ? hints->ai_protocol : 0;
    unsigned port=0, count=0, i, kind;
    unsigned long addresses[256];
    unsigned short ports[2];
    BOOL enabled[2]={FALSE,FALSE}, numeric=TRUE;
    const char *canonical=node;
    struct addrinfo *head=NULL, **tail=&head;
    if (!output) return EAI_FAIL;
    *output=NULL;
    if (!node && !service) return EAI_NONAME;
    if (flags & ~(AI_PASSIVE|AI_CANONNAME|AI_NUMERICHOST|AI_NUMERICSERV|AI_ADDRCONFIG)) return EAI_BADFLAGS;
    if (family != AF_UNSPEC && family != AF_INET) return EAI_FAMILY;
    if (type && type != SOCK_STREAM && type != SOCK_DGRAM) return EAI_SOCKTYPE;
    if (protocol && protocol != IPPROTO_TCP && protocol != IPPROTO_UDP) return EAI_SERVICE;
    if ((type == SOCK_STREAM && protocol == IPPROTO_UDP) ||
        (type == SOCK_DGRAM && protocol == IPPROTO_TCP)) return EAI_SOCKTYPE;
    if (service) {
        if (!*service) return EAI_SERVICE;
        for (i=0; service[i]; ++i) {
            if (service[i] < '0' || service[i] > '9') { numeric=FALSE; break; }
            if (port > 6553 || (port == 6553 && service[i] > '5')) return EAI_SERVICE;
            port=port*10+(unsigned)(service[i]-'0');
        }
    }
    if (!numeric && (flags & AI_NUMERICSERV)) return EAI_NONAME;
    for (kind=0; kind<2; ++kind) {
        int wanted_type=kind ? SOCK_DGRAM : SOCK_STREAM;
        int wanted_protocol=kind ? IPPROTO_UDP : IPPROTO_TCP;
        if ((type && type != wanted_type) || (protocol && protocol != wanted_protocol)) continue;
        if (numeric) ports[kind]=htons((u_short)port);
        else {
            struct servent *entry=getservbyname(service,kind ? "udp" : "tcp");
            if (!entry) continue;
            ports[kind]=(unsigned short)entry->s_port;
        }
        enabled[kind]=TRUE;
    }
    if (!enabled[0] && !enabled[1]) return EAI_SERVICE;
    if (!node) {
        addresses[count++]=htonl((flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK);
    } else {
        unsigned long address=inet_addr(node);
        if (address != INADDR_NONE || same(node,"255.255.255.255")) addresses[count++]=address;
        else {
            struct hostent *entry;
            if (flags & AI_NUMERICHOST) return EAI_NONAME;
            entry=gethostbyname(node);
            if (!entry) return dns_error(WSAGetLastError());
            if (entry->h_addrtype != AF_INET || entry->h_length != 4) return EAI_FAMILY;
            while (count < 256 && entry->h_addr_list[count]) {
                copy(&addresses[count],entry->h_addr_list[count],4); ++count;
            }
            if (!count) return EAI_NONAME;
            if (entry->h_name) canonical=entry->h_name;
        }
    }
    for (i=0; i<count; ++i) for (kind=0; kind<2; ++kind) if (enabled[kind]) {
        struct addrinfo *entry=allocate(sizeof(*entry));
        struct sockaddr_in *address=allocate(sizeof(*address));
        if (!entry || !address) {
            release(entry); release(address); rp_freeaddrinfo(head); return EAI_MEMORY;
        }
        entry->ai_family=AF_INET; entry->ai_socktype=kind ? SOCK_DGRAM : SOCK_STREAM;
        entry->ai_protocol=kind ? IPPROTO_UDP : IPPROTO_TCP;
        entry->ai_addrlen=sizeof(*address); entry->ai_addr=(struct sockaddr *)address;
        address->sin_family=AF_INET; address->sin_port=ports[kind]; address->sin_addr.s_addr=addresses[i];
        *tail=entry; tail=&entry->ai_next;
    }
    if ((flags & AI_CANONNAME) && canonical) {
        head->ai_canonname=duplicate(canonical);
        if (!head->ai_canonname) { rp_freeaddrinfo(head); return EAI_MEMORY; }
    }
    *output=head; return 0;
}

int WSAAPI rp_getnameinfo(const struct sockaddr *address, socklen_t size,
    char *host, DWORD host_size, char *service, DWORD service_size, int flags)
{
    const struct sockaddr_in *in=(const struct sockaddr_in *)address;
    char hostname[1025], service_name[256], *next;
    const unsigned char *bytes;
    struct hostent *entry=NULL;
    if (!address || size < (socklen_t)sizeof(*in)) return EAI_FAIL;
    if (address->sa_family != AF_INET) return EAI_FAMILY;
    if (flags & ~(NI_NUMERICHOST|NI_NUMERICSERV|NI_NAMEREQD|NI_DGRAM|NI_NOFQDN)) return EAI_BADFLAGS;
    if ((!host && host_size) || (!service && service_size)) return EAI_FAIL;
    if (host && host_size) {
        if (!(flags & NI_NUMERICHOST)) entry=gethostbyaddr((const char *)&in->sin_addr,4,AF_INET);
        if (entry && entry->h_name) {
            unsigned n=length(entry->h_name);
            if (n >= sizeof(hostname)) return EAI_OVERFLOW;
            copy(hostname,entry->h_name,n+1);
            if (flags & NI_NOFQDN) {
                char local[256];
                if (gethostname(local,sizeof(local)) == 0) {
                    unsigned local_length=length(local);
                    if (local_length && n >= local_length &&
                        (same(hostname,local) || (n > local_length && hostname[local_length] == '.'))) {
                        unsigned k;
                        for (k=0; k<local_length && hostname[k] == local[k]; ++k) {}
                        if (k == local_length) hostname[k]=0;
                    }
                }
            }
        } else {
            if ((flags & NI_NAMEREQD) && !(flags & NI_NUMERICHOST)) return EAI_NONAME;
            bytes=(const unsigned char *)&in->sin_addr;
            next=decimal(hostname,bytes[0]); *next++='.';
            next=decimal(next,bytes[1]); *next++='.';
            next=decimal(next,bytes[2]); *next++='.'; decimal(next,bytes[3]);
        }
        if (length(hostname)+1 > host_size) return EAI_OVERFLOW;
    }
    if (service && service_size) {
        struct servent *entry=(flags & NI_NUMERICSERV) ? NULL :
            getservbyport(in->sin_port,(flags & NI_DGRAM) ? "udp" : "tcp");
        if (entry && entry->s_name) {
            unsigned n=length(entry->s_name);
            if (n >= sizeof(service_name)) return EAI_OVERFLOW;
            copy(service_name,entry->s_name,n+1);
        } else decimal(service_name,ntohs(in->sin_port));
        if (length(service_name)+1 > service_size) return EAI_OVERFLOW;
    }
    if (host && host_size) copy(host,hostname,length(hostname)+1);
    if (service && service_size) copy(service,service_name,length(service_name)+1);
    return 0;
}

static int gather_size(LPWSABUF buffers, DWORD count, DWORD *total)
{
    DWORD i, result=0;
    if (!buffers || !count) { WSASetLastError(WSAEINVAL); return SOCKET_ERROR; }
    for (i=0; i<count; ++i) {
        if ((!buffers[i].buf && buffers[i].len) || buffers[i].len > INT_MAX-result) {
            WSASetLastError(WSAEFAULT); return SOCKET_ERROR;
        }
        result+=buffers[i].len;
    }
    *total=result; return 0;
}

int WSAAPI rp_sendto(SOCKET socket, LPWSABUF buffers, DWORD count, LPDWORD sent,
    DWORD flags, const struct sockaddr *to, int to_length,
    LPWSAOVERLAPPED overlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE completion)
{
    DWORD total, i, offset=0;
    char *flat;
    int result, error;
    if (overlapped || completion) { WSASetLastError(WSAEOPNOTSUPP); return SOCKET_ERROR; }
    if (!sent) { WSASetLastError(WSAEFAULT); return SOCKET_ERROR; }
    *sent=0;
    if (gather_size(buffers,count,&total)) return SOCKET_ERROR;
    flat=allocate(total ? total : 1);
    if (!flat) { WSASetLastError(WSAENOBUFS); return SOCKET_ERROR; }
    for (i=0; i<count; ++i) { copy(flat+offset,buffers[i].buf,buffers[i].len); offset+=buffers[i].len; }
    result=to ? sendto(socket,flat,(int)total,(int)flags,to,to_length) : send(socket,flat,(int)total,(int)flags);
    error=WSAGetLastError(); release(flat);
    if (result == SOCKET_ERROR) { WSASetLastError(error); return SOCKET_ERROR; }
    *sent=(DWORD)result; return 0;
}

int WSAAPI rp_recvfrom(SOCKET socket, LPWSABUF buffers, DWORD count, LPDWORD received,
    LPDWORD flags, struct sockaddr *from, LPINT from_length,
    LPWSAOVERLAPPED overlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE completion)
{
    DWORD total, i, offset=0;
    char *flat;
    int result, error;
    if (overlapped || completion) { WSASetLastError(WSAEOPNOTSUPP); return SOCKET_ERROR; }
    if (!received || !flags) { WSASetLastError(WSAEFAULT); return SOCKET_ERROR; }
    *received=0;
    if (gather_size(buffers,count,&total)) return SOCKET_ERROR;
    flat=allocate(total ? total : 1);
    if (!flat) { WSASetLastError(WSAENOBUFS); return SOCKET_ERROR; }
    result=from ? recvfrom(socket,flat,(int)total,(int)*flags,from,from_length) : recv(socket,flat,(int)total,(int)*flags);
    error=WSAGetLastError();
    if (result == SOCKET_ERROR) { release(flat); WSASetLastError(error); return SOCKET_ERROR; }
    for (i=0; i<count && offset<(DWORD)result; ++i) {
        DWORD copied=buffers[i].len;
        if (copied > (DWORD)result-offset) copied=(DWORD)result-offset;
        copy(buffers[i].buf,flat+offset,copied); offset+=copied;
    }
    release(flat); *received=(DWORD)result; *flags=0; return 0;
}

int WSAAPI rp_recv(SOCKET socket, LPWSABUF buffers, DWORD count, LPDWORD received,
    LPDWORD flags, LPWSAOVERLAPPED overlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE completion)
{
    return rp_recvfrom(socket,buffers,count,received,flags,NULL,NULL,overlapped,completion);
}

BOOL WINAPI RPEntry(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)module; (void)reason; (void)reserved; return TRUE;
}
