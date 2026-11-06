/* Wide directory enumeration for old NT and the Win9x ANSI filesystem APIs. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "WIDE.H"
typedef struct Search {
    struct Search *next;
    HANDLE handle;
    BOOL basic,directories;
} Search;
static HANDLE gate;
static Search *searches;
static HMODULE kernel;
static BOOL win9x;
static volatile BYTE *nt31_oem;

static volatile BYTE *oem_flag(void)
{
    /* NT3.1 exposes only the OEM setter. Its tiny native setter writes the
     * process-local mode byte; recognize the instruction and writable image
     * section rather than hard-coding an address or assuming ANSI forever. */
    BYTE *base=(BYTE *)kernel,*code=(BYTE *)GetProcAddress(kernel,"SetFileApisToOEM");
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base;
    if(!code || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)return NULL;
    IMAGE_NT_HEADERS32 *pe=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return NULL;
    DWORD size=pe->OptionalHeader.SizeOfImage;
    if((ULONG_PTR)code<(ULONG_PTR)base || (ULONG_PTR)code-(ULONG_PTR)base+8>size)return NULL;
    if(code[0]!=0xc6 || code[1]!=0x05 || code[6]!=1 || code[7]!=0xc3)return NULL;
    ULONG_PTR address=(DWORD)code[2]|((DWORD)code[3]<<8)|((DWORD)code[4]<<16)|((DWORD)code[5]<<24);
    if(address<(ULONG_PTR)base || address-(ULONG_PTR)base>=size)return NULL;
    IMAGE_SECTION_HEADER *section=IMAGE_FIRST_SECTION(pe);
    if(pe->FileHeader.NumberOfSections>96 ||
       (BYTE *)(section+pe->FileHeader.NumberOfSections)>base+pe->OptionalHeader.SizeOfHeaders)return NULL;
    for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i) {
        DWORD offset=(DWORD)(address-(ULONG_PTR)base),start=section[i].VirtualAddress;
        DWORD extent=section[i].Misc.VirtualSize;
        /* Original NT3.1 images leave VirtualSize zero. */
        if(extent<section[i].SizeOfRawData)extent=section[i].SizeOfRawData;
        if((section[i].Characteristics&IMAGE_SCN_MEM_WRITE) && offset>=start &&
           offset-start<extent)return (volatile BYTE *)address;
    }
    return NULL;
}

BOOL rp_find_init(void)
{
    kernel=GetModuleHandleA("KERNEL32.DLL");
    win9x=(GetVersion()&0x80000000UL)!=0;
    if(!win9x && !GetProcAddress(kernel,"AreFileApisANSI"))nt31_oem=oem_flag();
    gate=CreateMutexA(NULL,FALSE,NULL);
    return kernel && gate;
}
void rp_find_done(void)
{
    while(searches) { Search *next=searches->next;HeapFree(GetProcessHeap(),0,searches);searches=next; }
    if(gate)CloseHandle(gate);
}
BOOL WINAPI rp_file_apis_ansi(void)
{
    typedef BOOL (WINAPI *FUNCTION)(void);
    FUNCTION native=(FUNCTION)GetProcAddress(kernel,"AreFileApisANSI");
    if(native)return native();
    if(nt31_oem)return !*nt31_oem;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return TRUE;
}
void WINAPI rp_file_apis_oem(void)
{
    typedef void (WINAPI *FUNCTION)(void);
    FUNCTION native=(FUNCTION)GetProcAddress(kernel,"SetFileApisToOEM");
    if(native)native();else SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
}
void WINAPI rp_file_apis_ansi_set(void)
{
    typedef void (WINAPI *FUNCTION)(void);
    FUNCTION native=(FUNCTION)GetProcAddress(kernel,"SetFileApisToANSI");
    if(native)native();else if(nt31_oem)*nt31_oem=0;else SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
}
static BOOL convert(const WIN32_FIND_DATAA *ansi,WIN32_FIND_DATAW *wide)
{
    UINT cp=rp_file_apis_ansi()?CP_ACP:CP_OEMCP;
    wide->dwFileAttributes=ansi->dwFileAttributes;
    wide->ftCreationTime=ansi->ftCreationTime;wide->ftLastAccessTime=ansi->ftLastAccessTime;
    wide->ftLastWriteTime=ansi->ftLastWriteTime;
    wide->nFileSizeHigh=ansi->nFileSizeHigh;wide->nFileSizeLow=ansi->nFileSizeLow;
    wide->dwReserved0=ansi->dwReserved0;wide->dwReserved1=ansi->dwReserved1;
    return MultiByteToWideChar(cp,0,ansi->cFileName,-1,wide->cFileName,MAX_PATH) &&
           MultiByteToWideChar(cp,0,ansi->cAlternateFileName,-1,wide->cAlternateFileName,14);
}
HANDLE WINAPI rp_find_first(LPCWSTR name,LPWIN32_FIND_DATAW data)
{
    if(!name || !data) { SetLastError(ERROR_INVALID_PARAMETER);return INVALID_HANDLE_VALUE; }
    if(!win9x) {
        typedef HANDLE (WINAPI *FUNCTION)(LPCWSTR,LPWIN32_FIND_DATAW);
        FUNCTION native=(FUNCTION)GetProcAddress(kernel,"FindFirstFileW");
        return native(name,data);
    }
    char *path=rp_narrow_cp(name,rp_file_apis_ansi()?CP_ACP:CP_OEMCP);
    if(!path)return INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAA ansi;
    HANDLE handle=FindFirstFileA(path,&ansi);DWORD error=GetLastError();
    rp_free_name(path,error);
    if(handle!=INVALID_HANDLE_VALUE && !convert(&ansi,data)) {
        error=GetLastError();FindClose(handle);SetLastError(error);return INVALID_HANDLE_VALUE;
    }
    return handle;
}
static BOOL next_native(HANDLE handle,LPWIN32_FIND_DATAW data)
{
    if(!win9x) {
        typedef BOOL (WINAPI *FUNCTION)(HANDLE,LPWIN32_FIND_DATAW);
        FUNCTION native=(FUNCTION)GetProcAddress(kernel,"FindNextFileW");
        return native(handle,data);
    }
    WIN32_FIND_DATAA ansi;
    return FindNextFileA(handle,&ansi) && convert(&ansi,data);
}
BOOL WINAPI rp_find_next(HANDLE handle,LPWIN32_FIND_DATAW data)
{
    if(!data) { SetLastError(ERROR_INVALID_PARAMETER);return FALSE; }
    WaitForSingleObject(gate,INFINITE);
    Search *item;for(item=searches;item && item->handle!=handle;item=item->next) {}
    BOOL ok;
    do { ok=next_native(handle,data); }
    while(ok && item && item->directories && !(data->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY));
    if(ok && item && item->basic)data->cAlternateFileName[0]=0;
    DWORD error=GetLastError();ReleaseMutex(gate);SetLastError(error);return ok;
}
HANDLE WINAPI rp_find_ex(LPCWSTR name,FINDEX_INFO_LEVELS info,LPVOID output,
                         FINDEX_SEARCH_OPS operation,LPVOID filter,DWORD flags)
{
    if((int)info<0 || (int)info>1 || (int)operation<0 || (int)operation>2 || filter || (flags&~3UL)) {
        SetLastError(ERROR_INVALID_PARAMETER);return INVALID_HANDLE_VALUE;
    }
    if((flags&1) || (int)operation==2) {
        SetLastError(ERROR_NOT_SUPPORTED);return INVALID_HANDLE_VALUE;
    }
    /* LARGE_FETCH (2) is a performance hint. Basic and directory filtering
     * are retained through FindNextFileW, not just the first result. */
    WIN32_FIND_DATAW *data=output;
    HANDLE handle=rp_find_first(name,data);
    if(handle==INVALID_HANDLE_VALUE)return handle;
    if(operation==FindExSearchLimitToDirectories) {
        while(!(data->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
            if(!next_native(handle,data)) {
                DWORD error=GetLastError();FindClose(handle);SetLastError(error);return INVALID_HANDLE_VALUE;
            }
        }
    }
    if((int)info==1 || operation==FindExSearchLimitToDirectories) {
        Search *item=HeapAlloc(GetProcessHeap(),0,sizeof(*item));
        if(!item) { FindClose(handle);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE; }
        item->handle=handle;item->basic=(int)info==1;item->directories=operation==FindExSearchLimitToDirectories;
        if(item->basic)data->cAlternateFileName[0]=0;
        WaitForSingleObject(gate,INFINITE);item->next=searches;searches=item;ReleaseMutex(gate);
    }
    return handle;
}
BOOL WINAPI rp_find_close(HANDLE handle)
{
    WaitForSingleObject(gate,INFINITE);
    Search **link=&searches;
    while(*link && (*link)->handle!=handle)link=&(*link)->next;
    BOOL ok=FindClose(handle);DWORD error=GetLastError();
    if(ok && *link) { Search *item=*link;*link=item->next;HeapFree(GetProcessHeap(),0,item); }
    ReleaseMutex(gate);SetLastError(error);return ok;
}
