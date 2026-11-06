#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

static unsigned checks, failures, first_failure;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; if (!first_failure) first_failure=checks; } } while (0)
static struct addrinfo hints;

static int readable(SOCKET socket)
{
    fd_set set;
    struct timeval timeout={2,0};
    FD_ZERO(&set); FD_SET(socket,&set);
    return select(0,&set,NULL,NULL,&timeout) == 1;
}

void WINAPI RPEntry(void)
{
    WSADATA data;
    int startup=WSAStartup(MAKEWORD(1,1),&data), size, status;
    int socket_error=0;
    BOOL blocked=FALSE;
    struct addrinfo *list=NULL;
    struct sockaddr_in address, source;
    char host[64], service[16], first[4], second[8], path[MAX_PATH], text[256], *p, *end;
    WSABUF send_buffers[2], receive_buffers[2];
    SOCKET receiver=INVALID_SOCKET, sender=INVALID_SOCKET;
    DWORD transferred, flags, written;
    HANDLE report;
    CHECK(startup == 0);
    if (startup == 0) {
        CHECK(LOBYTE(data.wVersion) >= 1);
        hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
        hints.ai_flags=AI_NUMERICHOST|AI_CANONNAME;
        status=getaddrinfo("127.0.0.1","65535",&hints,&list);
        CHECK(status == 0 && list != NULL);
        if (list) {
            const struct sockaddr_in *in=(const struct sockaddr_in *)list->ai_addr;
            CHECK(list->ai_family == AF_INET && list->ai_protocol == IPPROTO_TCP && list->ai_next == NULL);
            CHECK(ntohs(in->sin_port) == 65535 && ntohl(in->sin_addr.s_addr) == INADDR_LOOPBACK);
            CHECK(list->ai_canonname && lstrcmpA(list->ai_canonname,"127.0.0.1") == 0);
            CHECK(getnameinfo(list->ai_addr,(int)list->ai_addrlen,host,sizeof(host),service,sizeof(service),NI_NUMERICHOST|NI_NUMERICSERV) == 0);
            CHECK(lstrcmpA(host,"127.0.0.1") == 0 && lstrcmpA(service,"65535") == 0);
            host[0]='?';
            CHECK(getnameinfo(list->ai_addr,(int)list->ai_addrlen,host,1,NULL,0,NI_NUMERICHOST) == WSAEFAULT && host[0] == '?');
            freeaddrinfo(list); list=NULL;
        }
        CHECK(getaddrinfo("127.0.0.1","65536",&hints,&list) == EAI_SERVICE && !list);
        CHECK(getaddrinfo("not-a-numeric-host","80",&hints,&list) == EAI_NONAME && !list);
        hints.ai_family=AF_INET6;
        CHECK(getaddrinfo("::1","80",&hints,&list) == EAI_FAMILY && !list);
        hints.ai_family=AF_INET; hints.ai_socktype=SOCK_DGRAM; hints.ai_flags=AI_PASSIVE;
        CHECK(getaddrinfo(NULL,"0",&hints,&list) == 0 && list != NULL);
        if (list) {
            CHECK(((struct sockaddr_in *)list->ai_addr)->sin_addr.s_addr == INADDR_ANY && list->ai_protocol == IPPROTO_UDP);
            freeaddrinfo(list); list=NULL;
        }
        receiver=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        sender=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (receiver == INVALID_SOCKET || sender == INVALID_SOCKET) {
            socket_error=WSAGetLastError();
            /* These are failures of the native Winsock socket function,
             * before any RPNET I/O wrapper is called. */
            blocked=socket_error==ERROR_FILE_NOT_FOUND || socket_error==WSAESOCKTNOSUPPORT ||
                    socket_error==WSAEPROTONOSUPPORT || socket_error==WSAEAFNOSUPPORT;
        }
        if (!blocked) CHECK(receiver != INVALID_SOCKET && sender != INVALID_SOCKET);
        if (receiver != INVALID_SOCKET && sender != INVALID_SOCKET) {
            address.sin_family=AF_INET; address.sin_port=0;
            address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
            status=bind(receiver,(struct sockaddr *)&address,sizeof(address));
            CHECK(status == 0);
            if (status == 0) {
                size=sizeof(address);
                CHECK(getsockname(receiver,(struct sockaddr *)&address,&size) == 0);
                send_buffers[0].buf="RET"; send_buffers[0].len=3;
                send_buffers[1].buf="RO42"; send_buffers[1].len=4;
                receive_buffers[0].buf=first; receive_buffers[0].len=3;
                receive_buffers[1].buf=second; receive_buffers[1].len=4;
                CHECK(WSASendTo(sender,send_buffers,2,&transferred,0,(struct sockaddr *)&address,sizeof(address),NULL,NULL) == 0 && transferred == 7);
                status=readable(receiver); CHECK(status);
                if (status) {
                    flags=0; size=sizeof(source);
                    CHECK(WSARecvFrom(receiver,receive_buffers,2,&transferred,&flags,(struct sockaddr *)&source,&size,NULL,NULL) == 0 && transferred == 7 && flags == 0);
                    CHECK(first[0]=='R' && first[1]=='E' && first[2]=='T' && second[0]=='R' && second[1]=='O' && second[2]=='4' && second[3]=='2');
                    CHECK(source.sin_family == AF_INET && ntohl(source.sin_addr.s_addr) == INADDR_LOOPBACK);
                }
                CHECK(connect(sender,(struct sockaddr *)&address,sizeof(address)) == 0);
                CHECK(WSASendTo(sender,send_buffers,2,&transferred,0,NULL,0,NULL,NULL) == 0 && transferred == 7);
                status=readable(receiver); CHECK(status);
                if (status) {
                    flags=0;
                    CHECK(WSARecv(receiver,receive_buffers,2,&transferred,&flags,NULL,NULL) == 0 && transferred == 7);
                    CHECK(first[0]=='R' && second[3]=='2');
                }
                CHECK(WSASendTo(sender,send_buffers,2,NULL,0,NULL,0,NULL,NULL) == SOCKET_ERROR && WSAGetLastError() == WSAEFAULT);
            }
        }
        if (!socket_error) socket_error=WSAGetLastError();
        if (receiver != INVALID_SOCKET) closesocket(receiver);
        if (sender != INVALID_SOCKET) closesocket(sender);
        CHECK(WSACleanup() == 0);
    }
    size=(int)GetModuleFileNameA(NULL,path,MAX_PATH);
    if (!size || size >= MAX_PATH-16) ExitProcess(2);
    end=path;
    for (p=path; *p; ++p) if (*p=='\\') end=p+1;
    lstrcpyA(end,"NETTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report == INVALID_HANDLE_VALUE) ExitProcess(3);
    wsprintfA(text,"RPNET IPv4 synchronous I/O test\r\nCHECKS=%u FAILURES=%u FIRST=%u\r\nSTARTUP=%d LAST_WSA_ERROR=%d\r\nNATIVE_PROVIDER_BLOCKED=%u\r\nRESULT=%s\r\n",
        checks,failures,first_failure,startup,socket_error,blocked,failures ? "FAIL" : blocked ? "BLOCKED" : "PASS");
    if (!WriteFile(report,text,(DWORD)lstrlenA(text),&written,NULL) || written!=(DWORD)lstrlenA(text)) ExitProcess(4);
    CloseHandle(report); ExitProcess(failures ? 1 : blocked ? 77 : 0);
}
