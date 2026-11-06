/* Deterministically preempt the real atomic/synchronization gate owner.
 * A higher-priority watchdog bounds the old Sleep(0) failure and restores
 * scheduling priority before joining either worker. */
#define RPEntry RPUnusedEntry
#ifdef RP_PRI_OLD
#include "../BUILD/PRIBASE/ATOM.C"
#elif defined(RP_PRI_GATE)
#include "../SOURCES/ATGATE2.C"
#else
#include "../SRC/ATOM.C"
#endif
#undef RPEntry
/* The gate-only test never calls the CV/critical-section helpers. Supply
 * native CS forwarding for those unrelated routines in the included file. */
void WINAPI rp_cs_enter(LPCRITICAL_SECTION cs) { EnterCriticalSection(cs); }
void WINAPI rp_cs_leave(LPCRITICAL_SECTION cs) { LeaveCriticalSection(cs); }
#define gate sync_gate
#ifdef RP_PRI_OLD
#include "../BUILD/PRIBASE/SYNC.C"
#elif defined(RP_PRI_GATE)
#include "../SOURCES/SYGATE2.C"
#else
#include "../SRC/SYNC.C"
#endif
#undef gate
static HANDLE held,release_owner,started,report;
static unsigned mode,checks,failures;
#define CHECK(x) do{++checks;if(!(x))++failures;}while(0)
static void line(const char *text)
{
    DWORD n;WriteFile(report,text,lstrlenA(text),&n,NULL);FlushFileBuffers(report);
}
static void lock_gate(void) {if(mode)enter_gate();else acquire();}
static void unlock_gate(void) {if(mode)leave_gate();else release();}
static DWORD WINAPI owner(void *unused)
{
    (void)unused;lock_gate();SetEvent(held);
    WaitForSingleObject(release_owner,INFINITE);unlock_gate();return 0;
}
static DWORD WINAPI waiter(void *unused)
{
    (void)unused;SetEvent(started);lock_gate();unlock_gate();return 0;
}
void WINAPI RPEntry(void)
{
    char path[MAX_PATH],text[160],*last;DWORD id,n=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!n||n>=MAX_PATH-20)ExitProcess(2);
    last=path;for(char *p=path;*p;++p)if(*p=='\\')last=p+1;
#ifdef RP_PRI_OLD
    lstrcpyA(last,"PRIOLD.TXT");
#else
    lstrcpyA(last,"PRITEST.TXT");
#endif
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
#ifdef RP_PRI_GATE
    RPUnusedEntry(NULL,DLL_PROCESS_ATTACH,NULL);
    rp_sync_init();
#endif
    line("Scope: actual private gate code, preempted owner and bounded priority watchdog.\r\n");
    CHECK(SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL));
    if(failures)goto done;
    for(mode=0;mode<2;++mode){
        unsigned previous_failures=failures;
        held=CreateEventA(NULL,TRUE,FALSE,NULL);
        release_owner=CreateEventA(NULL,TRUE,FALSE,NULL);
        started=CreateEventA(NULL,TRUE,FALSE,NULL);
        if(!held||!release_owner||!started)ExitProcess(4);
        HANDLE workers[2];
        workers[0]=CreateThread(NULL,0,owner,NULL,CREATE_SUSPENDED,&id);
        workers[1]=CreateThread(NULL,0,waiter,NULL,CREATE_SUSPENDED,&id);
        if(!workers[0]||!workers[1])ExitProcess(4);
        CHECK(SetThreadPriority(workers[0],THREAD_PRIORITY_NORMAL));
        CHECK(SetThreadPriority(workers[1],THREAD_PRIORITY_HIGHEST));
        if(failures!=previous_failures)ExitProcess(4);
        CHECK(ResumeThread(workers[0])!=0xffffffffUL);
        CHECK(WaitForSingleObject(held,3000)==WAIT_OBJECT_0);
        CHECK(ResumeThread(workers[1])!=0xffffffffUL);
        CHECK(WaitForSingleObject(started,3000)==WAIT_OBJECT_0);
        DWORD before=GetTickCount();SetEvent(release_owner);
        DWORD result=WaitForMultipleObjects(2,workers,TRUE,1000);
        DWORD elapsed=GetTickCount()-before;
        CHECK(result-WAIT_OBJECT_0<2);
        /* Recover even a broken implementation before attempting to join. */
        CHECK(SetThreadPriority(workers[1],THREAD_PRIORITY_BELOW_NORMAL));
        result=WaitForMultipleObjects(2,workers,TRUE,3000);
        CHECK(result-WAIT_OBJECT_0<2);
        if(result-WAIT_OBJECT_0>=2)ExitProcess(5);
        for(unsigned i=0;i<2;++i)CloseHandle(workers[i]);
        CloseHandle(held);CloseHandle(release_owner);CloseHandle(started);
        wsprintfA(text,"GATE=%s WAIT_MS=%lu\r\n",mode?"SYNC":"ATOM",elapsed);line(text);
    }
done:
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);
#ifdef RP_PRI_GATE
    rp_sync_done();
    RPUnusedEntry(NULL,DLL_PROCESS_DETACH,NULL);
#endif
    wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
    line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
