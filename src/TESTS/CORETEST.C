#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef int (__cdecl *CLONE)(void **, void *(__cdecl *)(void *), void *, int);
typedef void (__cdecl *JOIN)(void *,void **);
typedef void (__cdecl *CANCEL)(void *);
typedef void (__cdecl *SLEEP)(long long);
typedef void *(__cdecl *CREATE)(int,const char *const *);
typedef void (__cdecl *RELEASE)(void *);
typedef const char *(__cdecl *STRING)(void);
static HANDLE report;
static HMODULE dependency_modules[5];
static SLEEP sleep_vlc;
static volatile LONG started;
static unsigned checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) ++failures; } while (0)

static void line(const char *text)
{
    DWORD size=(DWORD)lstrlenA(text), written;
    if (!WriteFile(report,text,size,&written,NULL) || written!=size) ExitProcess(8);
    FlushFileBuffers(report);
}
static void * __cdecl worker(void *value) { return value; }
static LONG WINAPI exception_report(EXCEPTION_POINTERS *exception)
{
    char text[192];
    wsprintfA(text,"EXCEPTION=%08lx ADDRESS=%08lx CORE_BASE=%08lx\r\nRESULT=FAIL\r\n",
        exception->ExceptionRecord->ExceptionCode,
        (DWORD)exception->ExceptionRecord->ExceptionAddress,
        (DWORD)GetModuleHandleA("LIBCORE.DLL"));
    line(text);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void * __cdecl blocked(void *value)
{
    (void)value; started=1; sleep_vlc(60000000LL); return (void *)1;
}

void WINAPI RPEntry(void)
{
    char path[MAX_PATH], text[256], *p, *last;
    DWORD length, before, elapsed;
    HMODULE library, core;
    CLONE clone;
    JOIN join;
    CANCEL cancel;
    CREATE create;
    RELEASE release;
    STRING version, error;
    void *thread, *result, *instance;
    unsigned i;
    const char *args[]={"--ignore-config"
#if RP_USE_CACHE
        ,"--plugins-cache","--no-plugins-scan"
#endif
    };
    length=GetModuleFileNameA(NULL,path,MAX_PATH);
    if (!length || length>=MAX_PATH-20) ExitProcess(2);
    last=path;
    for (p=path; *p; ++p) if (*p=='\\') last=p+1;
    lstrcpyA(last,"CORETEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report==INVALID_HANDLE_VALUE) ExitProcess(3);
    SetUnhandledExceptionFilter(exception_report);
    line("Scope: core load, initialization and threads; playback is untested.\r\nSTEP=LOAD\r\n");
    {
        const char *dependencies[]={"RPATOM.DLL","RPKERN.DLL","RPCRT.DLL","RPNET.DLL","LIBCORE.DLL"};
        for (i=0; i<sizeof(dependencies)/sizeof(dependencies[0]); ++i) {
            lstrcpyA(last,dependencies[i]);
            dependency_modules[i]=LoadLibraryA(path);
            if (!dependency_modules[i]) {
                wsprintfA(text,"DEPENDENCY=%s LOAD_ERROR=%lu\r\nRESULT=FAIL\r\n",dependencies[i],GetLastError());
                line(text); ExitProcess(4);
            }
        }
    }
    lstrcpyA(last,"LIBVLC.DLL");
    library=LoadLibraryA(path);
    if (!library) {
        wsprintfA(text,"LOAD_ERROR=%lu\r\nRESULT=FAIL\r\n",GetLastError()); line(text); ExitProcess(4);
    }
    line("LOAD=PASS\r\n");
    core=GetModuleHandleA("LIBCORE.DLL");
    clone=(CLONE)GetProcAddress(core,"vlc_clone");
    join=(JOIN)GetProcAddress(core,"vlc_join");
    cancel=(CANCEL)GetProcAddress(core,"vlc_cancel");
    sleep_vlc=(SLEEP)GetProcAddress(core,"msleep");
    create=(CREATE)GetProcAddress(library,"libvlc_new");
    release=(RELEASE)GetProcAddress(library,"libvlc_release");
    version=(STRING)GetProcAddress(library,"libvlc_get_version");
    error=(STRING)GetProcAddress(library,"libvlc_errmsg");
    CHECK(clone && join && cancel && sleep_vlc && create && release && version && error);
    if (failures) { line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(5); }
    line("VERSION="); line(version()); line("\r\nSTEP=CREATE\r\n");
    instance=create(sizeof(args)/sizeof(args[0]),args);
    CHECK(instance != NULL);
    if (!instance) {
        const char *message=error();
        line("CREATE=FAIL\r\n"); if (message) { line(message); line("\r\n"); }
    } else line("CREATE=PASS\r\n");
    line("STEP=THREADS\r\n");
    for (i=0; i<100; ++i) {
        int status=clone(&thread,worker,(void *)(i+1),0);
        CHECK(status==0);
        if (!status) { join(thread,&result); CHECK(result==(void *)(i+1)); }
    }
    line("STEP=CANCEL\r\n");
    if (clone(&thread,blocked,NULL,0)==0) {
        before=GetTickCount();
        while (!started && GetTickCount()-before<5000) Sleep(1);
        CHECK(started);
        before=GetTickCount();
        cancel(thread); join(thread,&result);
        elapsed=GetTickCount()-before;
        CHECK(result==NULL && elapsed<2000);
        wsprintfA(text,"CANCEL_MS=%lu\r\n",elapsed); line(text);
    } else CHECK(FALSE);
    if (instance) { line("STEP=RELEASE\r\n"); release(instance); }
    line("STEP=UNLOAD\r\n");
    FreeLibrary(library);
    for (i=5; i>0; --i) FreeLibrary(dependency_modules[i-1]);
    wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures ? "FAIL" : "PASS");
    line(text); CloseHandle(report); ExitProcess(failures ? 1 : 0);
}
