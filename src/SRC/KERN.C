/* Legacy Win32 operations used by the VLC runtime.
 * Critical sections in this module are backed by recursive kernel mutexes.
 * All five critical-section imports must be routed together; do not pass
 * these objects to native condition-variable APIs or native CS functions.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "WIDE.H"

static HMODULE native_kernel;
static BOOL windows_9x;
extern BOOL rp_find_init(void);
extern void rp_find_done(void);

static void fatal(DWORD code)
{
    RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, NULL);
    ExitProcess(code);
}

void WINAPI rp_cs_init(LPCRITICAL_SECTION cs)
{
    HANDLE mutex = CreateMutexA(NULL, FALSE, NULL);
    if (!mutex) fatal(0xc0000017UL); /* STATUS_NO_MEMORY */
    cs->DebugInfo = NULL;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = NULL;
    cs->LockSemaphore = mutex;
    cs->SpinCount = 0;
}

void WINAPI rp_cs_enter(LPCRITICAL_SECTION cs)
{
    DWORD result = WaitForSingleObject(cs->LockSemaphore, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
        fatal(0xc0000008UL); /* STATUS_INVALID_HANDLE */
}

BOOL WINAPI rp_cs_try(LPCRITICAL_SECTION cs)
{
    DWORD result = WaitForSingleObject(cs->LockSemaphore, 0);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) return TRUE;
    if (result != WAIT_TIMEOUT) fatal(0xc0000008UL);
    return FALSE;
}

void WINAPI rp_cs_leave(LPCRITICAL_SECTION cs)
{
    if (!ReleaseMutex(cs->LockSemaphore)) fatal(0xc0000026UL);
}

void WINAPI rp_cs_delete(LPCRITICAL_SECTION cs)
{
    if (!CloseHandle(cs->LockSemaphore)) fatal(0xc0000008UL);
    cs->LockSemaphore = NULL;
}

void WINAPI rp_system_info(LPSYSTEM_INFO info)
{
    GetSystemInfo(info);
}

BOOL WINAPI rp_affinity(HANDLE process, PDWORD_PTR process_mask, PDWORD_PTR system_mask)
{
    typedef BOOL (WINAPI *AFFINITY)(HANDLE,PDWORD_PTR,PDWORD_PTR);
    typedef LONG (WINAPI *QUERY)(HANDLE,ULONG,PVOID,ULONG,PULONG);
    typedef ULONG (WINAPI *DOS_ERROR)(LONG);
    struct { LONG status; void *peb; ULONG mask; LONG priority; ULONG pid, parent; } basic;
    AFFINITY native=(AFFINITY)GetProcAddress(native_kernel,"GetProcessAffinityMask");
    HMODULE ntdll;
    QUERY query;
    DOS_ERROR translate;
    SYSTEM_INFO info;
    LONG status;
    if (!process_mask || !system_mask) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (native) return native(process,process_mask,system_mask);
    /* NT 3.1 exposes this information through its native process query. */
    ntdll=GetModuleHandleA("NTDLL.DLL");
    query=ntdll ? (QUERY)GetProcAddress(ntdll,"NtQueryInformationProcess") : NULL;
    if (!query) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    status=query(process,0,&basic,sizeof(basic),NULL);
    if (status<0) {
        translate=(DOS_ERROR)GetProcAddress(ntdll,"RtlNtStatusToDosError");
        SetLastError(translate ? translate(status) : ERROR_INVALID_HANDLE);
        return FALSE;
    }
    GetSystemInfo(&info);
    *process_mask=basic.mask; *system_mask=info.dwActiveProcessorMask;
    return TRUE;
}

void WINAPI rp_file_time(LPFILETIME result)
{
    SYSTEMTIME utc;
    GetSystemTime(&utc);
    if (!SystemTimeToFileTime(&utc, result)) fatal(0xc000000dUL);
}

BOOL WINAPI rp_dbcs(UINT codepage, BYTE value)
{
    CPINFO info;
    unsigned i;
    if (!GetCPInfo(codepage, &info)) return FALSE;
    for (i = 0; i + 1 < MAX_LEADBYTES && info.LeadByte[i]; i += 2)
        if (value >= info.LeadByte[i] && value <= info.LeadByte[i + 1])
            return TRUE;
    return FALSE;
}

HANDLE WINAPI rp_create_file(LPCWSTR name, DWORD access, DWORD sharing,
    LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD attributes, HANDLE template)
{
    typedef HANDLE (WINAPI *FUNCTION)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
    char *ansi;
    HANDLE result;
    DWORD error;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "CreateFileW");
        return function(name, access, sharing, security, creation, attributes, template);
    }
    ansi = rp_narrow(name);
    if (!ansi) return INVALID_HANDLE_VALUE;
    /* 9x supports read/write sharing but not FILE_SHARE_DELETE or NT security
     * quality-of-service bits. Neither has useful semantics on its kernel. */
    result = CreateFileA(ansi, access, sharing & ~FILE_SHARE_DELETE, security,
                        creation, attributes & ~SECURITY_VALID_SQOS_FLAGS, template);
    error = GetLastError(); rp_free_name(ansi, error);
    return result == NULL ? INVALID_HANDLE_VALUE : result;
}

HANDLE WINAPI rp_create_mutex(LPSECURITY_ATTRIBUTES security, BOOL owner, LPCWSTR name)
{
    typedef HANDLE (WINAPI *FUNCTION)(LPSECURITY_ATTRIBUTES,BOOL,LPCWSTR);
    char *ansi = NULL;
    HANDLE result;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "CreateMutexW");
        return function(security, owner, name);
    }
    if (name && !(ansi = rp_narrow(name))) return NULL;
    result = CreateMutexA(security, owner, ansi);
    rp_free_name(ansi, GetLastError());
    return result;
}

HANDLE WINAPI rp_create_semaphore(LPSECURITY_ATTRIBUTES security, LONG initial,
                                  LONG maximum, LPCWSTR name)
{
    typedef HANDLE (WINAPI *FUNCTION)(LPSECURITY_ATTRIBUTES,LONG,LONG,LPCWSTR);
    char *ansi = NULL;
    HANDLE result;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "CreateSemaphoreW");
        return function(security, initial, maximum, name);
    }
    if (name && !(ansi = rp_narrow(name))) return NULL;
    result = CreateSemaphoreA(security, initial, maximum, ansi);
    rp_free_name(ansi, GetLastError());
    return result;
}

HMODULE WINAPI rp_module_handle(LPCWSTR name)
{
    typedef HMODULE (WINAPI *FUNCTION)(LPCWSTR);
    char *ansi = NULL;
    HMODULE result;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "GetModuleHandleW");
        return function(name);
    }
    if (name && !(ansi = rp_narrow(name))) return NULL;
    result = GetModuleHandleA(ansi);
    rp_free_name(ansi, GetLastError());
    return result;
}

HMODULE WINAPI rp_load_ex(LPCWSTR name, HANDLE file, DWORD flags)
{
    typedef HMODULE (WINAPI *FUNCTION)(LPCWSTR,HANDLE,DWORD);
    char *ansi;
    HMODULE result;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "LoadLibraryExW");
        return function(name, file, flags);
    }
    ansi = rp_narrow(name);
    if (!ansi) return NULL;
    result = LoadLibraryExA(ansi, file, flags);
    rp_free_name(ansi, GetLastError());
    return result;
}

HMODULE WINAPI rp_load(LPCWSTR name)
{
    return rp_load_ex(name, NULL, 0);
}

DWORD WINAPI rp_module_name(HMODULE module, LPWSTR output, DWORD capacity)
{
    typedef DWORD (WINAPI *FUNCTION)(HMODULE,LPWSTR,DWORD);
    char narrow[MAX_PATH];
    WCHAR wide[MAX_PATH];
    DWORD length, i;
    int required;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "GetModuleFileNameW");
        return function(module, output, capacity);
    }
    if (!capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    if (!output) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    length = GetModuleFileNameA(module, narrow, MAX_PATH);
    if (!length) return 0;
    if (length >= MAX_PATH) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return 0; }
    required = MultiByteToWideChar(CP_ACP, 0, narrow, -1, wide, MAX_PATH);
    if (!required) return 0;
    if ((DWORD)required > capacity) {
        for (i = 0; i+1 < capacity; ++i) output[i] = wide[i];
        output[capacity-1] = 0;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return capacity;
    }
    for (i = 0; i < (DWORD)required; ++i) output[i] = wide[i];
    return (DWORD)required-1;
}

DWORD WINAPI rp_attributes(LPCWSTR name)
{
    typedef DWORD (WINAPI *FUNCTION)(LPCWSTR);
    char *ansi;
    DWORD result;
    if (!windows_9x) {
        FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "GetFileAttributesW");
        return function(name);
    }
    ansi = rp_narrow(name);
    if (!ansi) return INVALID_FILE_ATTRIBUTES;
    result = GetFileAttributesA(ansi);
    rp_free_name(ansi, GetLastError());
    return result;
}

BOOL WINAPI rp_processor_feature(DWORD feature)
{
    typedef BOOL (WINAPI *FUNCTION)(DWORD);
    FUNCTION function = (FUNCTION)GetProcAddress(native_kernel, "IsProcessorFeaturePresent");
    /* A CPU flag alone does not establish OS support for saving extended
     * register state or dispatching fast-fail exceptions. On pre-API systems
     * advertise no optional OS feature; VLC/codec CPU probing is independent. */
    return function ? function(feature) : FALSE;
}

DWORD WINAPI rp_full_path(LPCWSTR name, DWORD capacity, LPWSTR output, LPWSTR *file_part)
{
    typedef DWORD (WINAPI *FUNCTION)(LPCWSTR,DWORD,LPWSTR,LPWSTR *);
    char *input, *absolute;
    DWORD required, length, error;
    int wide_count;
    WCHAR *part, *p;
    if (!windows_9x) {
        FUNCTION function=(FUNCTION)GetProcAddress(native_kernel,"GetFullPathNameW");
        return function(name,capacity,output,file_part);
    }
    if (!name || (capacity && !output)) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    input=rp_narrow(name);
    if (!input) return 0;
    required=GetFullPathNameA(input,0,NULL,NULL);
    if (!required) { error=GetLastError(); rp_free_name(input,error); return 0; }
    absolute=HeapAlloc(GetProcessHeap(),0,required);
    if (!absolute) { rp_free_name(input,ERROR_NOT_ENOUGH_MEMORY); return 0; }
    length=GetFullPathNameA(input,required,absolute,NULL);
    error=GetLastError(); rp_free_name(input,error);
    if (!length || length>=required) {
        HeapFree(GetProcessHeap(),0,absolute);
        SetLastError(length ? ERROR_INSUFFICIENT_BUFFER : error); return 0;
    }
    wide_count=MultiByteToWideChar(CP_ACP,0,absolute,-1,NULL,0);
    if (!wide_count) { error=GetLastError(); HeapFree(GetProcessHeap(),0,absolute); SetLastError(error); return 0; }
    if ((DWORD)wide_count>capacity) {
        HeapFree(GetProcessHeap(),0,absolute);
        return (DWORD)wide_count;
    }
    if (!MultiByteToWideChar(CP_ACP,0,absolute,-1,output,wide_count)) {
        error=GetLastError(); HeapFree(GetProcessHeap(),0,absolute); SetLastError(error); return 0;
    }
    HeapFree(GetProcessHeap(),0,absolute);
    if (file_part) {
        part=output;
        for (p=output; *p; ++p) if (*p=='\\' || *p=='/') part=p+1;
        *file_part=*part ? part : NULL;
    }
    return (DWORD)wide_count-1;
}

BOOL WINAPI RPEntry(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        native_kernel = GetModuleHandleA("KERNEL32.DLL");
        if (!native_kernel) return FALSE;
        windows_9x = (GetVersion() & 0x80000000UL) != 0;
        if(!rp_find_init())return FALSE;
    }
    else if(reason==DLL_PROCESS_DETACH)rp_find_done();
    return TRUE;
}
