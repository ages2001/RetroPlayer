/* Exercise real LibVLC condition waits, cancellation and the first clock read. */
#define WIN32_LEAN_AND_MEAN
#include "config.h"
#include <windows.h>
#include <vlc_common.h>
#include <errno.h>
static vlc_mutex_t mutex;
static vlc_cond_t request_condition, response_condition;
static unsigned request, response, checks, failures;
static HANDLE report, ready;
#define CHECK(x) do { ++checks; if(!(x)) ++failures; } while(0)
static void line(const char *s) { DWORD n; WriteFile(report,s,lstrlenA(s),&n,NULL); FlushFileBuffers(report); }
static DWORD WINAPI watchdog(void *unused)
{
    (void)unused; Sleep(30000); line("WATCHDOG=TIMEOUT\r\nRESULT=FAIL\r\n"); ExitProcess(9); return 0;
}
static void *exchange(void *unused)
{
    (void)unused;
    vlc_mutex_lock(&mutex);
    for(unsigned i=1;i<=256;++i) {
        while(request<i) vlc_cond_wait(&request_condition,&mutex);
        response=request;
        vlc_cond_signal(&response_condition);
    }
    vlc_mutex_unlock(&mutex);
    return NULL;
}
static void unlock(void *lock) { vlc_mutex_unlock(lock); }
static void *cancel_wait(void *unused)
{
    (void)unused;
    vlc_mutex_lock(&mutex);
    vlc_cleanup_push(unlock,&mutex);
    SetEvent(ready);
    for(;;) vlc_cond_wait(&request_condition,&mutex);
    vlc_cleanup_pop();
    return NULL;
}
void WINAPI RPEntry(void)
{
    char path[MAX_PATH], *last=path, text[192];
    DWORD id, size=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!size || size>=MAX_PATH-16) ExitProcess(2);
    for(char *p=path;*p;++p) if(*p=='\\') last=p+1;
    *last=0; lstrcatA(path,"CWTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE) ExitProcess(3);
    HANDLE guard=CreateThread(NULL,0,watchdog,NULL,0,&id);
    if(!guard) ExitProcess(4);
    CloseHandle(guard);
    line("Scope: cold clock, real core condition exchange, timeout and cancellation cleanup.\r\n");
    vlc_tick_t first=mdate(); Sleep(20); vlc_tick_t second=mdate();
    CHECK(first>0 && second>=first && second-first>=1000 && second-first<1000000);
    wsprintfA(text,"FIRST_CLOCK_DELTA_MS=%ld\r\n",(long)((second-first)/1000)); line(text);
    vlc_mutex_init(&mutex);
    vlc_cond_init(&request_condition); vlc_cond_init(&response_condition);
    vlc_thread_t thread;
    if(vlc_clone(&thread,exchange,NULL,VLC_THREAD_PRIORITY_LOW)) ExitProcess(4);
    DWORD before=GetTickCount();
    vlc_mutex_lock(&mutex);
    for(unsigned i=1;i<=256;++i) {
        request=i; vlc_cond_signal(&request_condition);
        vlc_tick_t deadline=mdate()+1000000;
        while(response<i) {
            int rc=vlc_cond_timedwait(&response_condition,&mutex,deadline);
            if(rc==ETIMEDOUT) { line("EXCHANGE=TIMEOUT\r\nRESULT=FAIL\r\n"); ExitProcess(5); }
        }
        CHECK(response==i);
    }
    vlc_mutex_unlock(&mutex);
    void *result=(void *)1; vlc_join(thread,&result); CHECK(result==NULL);
    wsprintfA(text,"EXCHANGES=256 ELAPSED_MS=%lu\r\n",GetTickCount()-before); line(text);
    vlc_mutex_lock(&mutex);
    vlc_tick_t deadline=mdate()+40000; unsigned wakes=0; int rc;
    before=GetTickCount();
    do { rc=vlc_cond_timedwait(&response_condition,&mutex,deadline); ++wakes; } while(rc==0);
    CHECK(rc==ETIMEDOUT); CHECK(GetTickCount()-before>=20 && GetTickCount()-before<1000);
    vlc_mutex_unlock(&mutex);
    wsprintfA(text,"TIMEOUT_MS=%lu WAIT_CALLS=%u\r\n",GetTickCount()-before,wakes); line(text);
    ready=CreateEventA(NULL,TRUE,FALSE,NULL); if(!ready) ExitProcess(4);
    for(unsigned i=0;i<16;++i) {
        ResetEvent(ready);
        if(vlc_clone(&thread,cancel_wait,NULL,VLC_THREAD_PRIORITY_LOW)) ExitProcess(4);
        CHECK(WaitForSingleObject(ready,3000)==WAIT_OBJECT_0);
        vlc_cancel(thread); result=(void *)1; vlc_join(thread,&result); CHECK(result==NULL);
        int acquired=vlc_mutex_trylock(&mutex); CHECK(acquired==0);
        if(!acquired) vlc_mutex_unlock(&mutex);
    }
    CloseHandle(ready); vlc_cond_destroy(&request_condition); vlc_cond_destroy(&response_condition);
    vlc_mutex_destroy(&mutex);
    wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");line(text);
    CloseHandle(report); ExitProcess(failures?1:0);
}
