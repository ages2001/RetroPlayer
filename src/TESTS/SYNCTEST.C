/* Contention, lost-wakeup, timeout and once-publication checks on the guest. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static SRWLOCK lock=SRWLOCK_INIT;
static CONDITION_VARIABLE condition=CONDITION_VARIABLE_INIT;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static unsigned ready,finished,counter,initialized,published;
static BOOL start;
static unsigned request,response;
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)) ++failures; } while(0)

static BOOL WINAPI initialize(PINIT_ONCE object,PVOID parameter,PVOID *context)
{
    (void)object; (void)parameter;
    ++initialized; Sleep(5); published=0x12345678; *context=&published;
    return TRUE;
}
static DWORD WINAPI worker(void *unused)
{
    unsigned i;
    void *context=NULL;
    (void)unused;
    AcquireSRWLockExclusive(&lock);
    ++ready; WakeAllConditionVariable(&condition);
    while(!start) if(!SleepConditionVariableSRW(&condition,&lock,10000,0)) {
        ReleaseSRWLockExclusive(&lock); return 1;
    }
    ReleaseSRWLockExclusive(&lock);
    if(!InitOnceExecuteOnce(&once,initialize,NULL,&context) || context!=&published ||
        published!=0x12345678) return 2;
    for(i=0;i<1000;++i) {
        AcquireSRWLockExclusive(&lock); ++counter; ReleaseSRWLockExclusive(&lock);
        AcquireSRWLockShared(&lock);
        if(!counter) { ReleaseSRWLockShared(&lock); return 3; }
        ReleaseSRWLockShared(&lock);
    }
    AcquireSRWLockExclusive(&lock);
    ++finished; WakeAllConditionVariable(&condition);
    ReleaseSRWLockExclusive(&lock); return 0;
}
static DWORD WINAPI exchange(void *unused)
{
    unsigned i;
    (void)unused;
    AcquireSRWLockExclusive(&lock);
    for(i=1;i<=500;++i) {
        while(request<i) if(!SleepConditionVariableSRW(&condition,&lock,5000,0)) {
            ReleaseSRWLockExclusive(&lock); return 1;
        }
        response=request; WakeConditionVariable(&condition);
    }
    ReleaseSRWLockExclusive(&lock); return 0;
}
static BOOL WINAPI retry(PINIT_ONCE object,PVOID parameter,PVOID *context)
{
    unsigned *calls=parameter;
    (void)object;
    if(!(*calls)++) { SetLastError(ERROR_RETRY); return FALSE; }
    *context=(void *)0x1000; return TRUE;
}
void WINAPI RPEntry(void)
{
    HANDLE threads[8],thread,file;
    DWORD id,code,length,written;
    unsigned i,count=0,calls=0;
    BOOL pending;
    void *context;
    CRITICAL_SECTION cs;
    INIT_ONCE other=INIT_ONCE_STATIC_INIT;
    char path[MAX_PATH],text[256],*p,*last;
    InitializeSRWLock(&lock); InitializeConditionVariable(&condition);
    CHECK(TryAcquireSRWLockExclusive(&lock));
    CHECK(!TryAcquireSRWLockExclusive(&lock));
    CHECK(!TryAcquireSRWLockShared(&lock));
    CHECK(!SleepConditionVariableSRW(&condition,&lock,0,2) && GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(!SleepConditionVariableSRW(&condition,&lock,15,0) && GetLastError()==ERROR_TIMEOUT);
    CHECK(!TryAcquireSRWLockExclusive(&lock));
    ReleaseSRWLockExclusive(&lock);
    CHECK(TryAcquireSRWLockShared(&lock));
    CHECK(TryAcquireSRWLockShared(&lock));
    CHECK(!TryAcquireSRWLockExclusive(&lock));
    ReleaseSRWLockShared(&lock);
    CHECK(!SleepConditionVariableSRW(&condition,&lock,0,CONDITION_VARIABLE_LOCKMODE_SHARED) && GetLastError()==ERROR_TIMEOUT);
    CHECK(!TryAcquireSRWLockExclusive(&lock));
    ReleaseSRWLockShared(&lock);
    InitializeCriticalSection(&cs); EnterCriticalSection(&cs);
    CHECK(!SleepConditionVariableCS(&condition,&cs,15) && GetLastError()==ERROR_TIMEOUT);
    CHECK(TryEnterCriticalSection(&cs)); LeaveCriticalSection(&cs); LeaveCriticalSection(&cs);
    DeleteCriticalSection(&cs);
    for(i=0;i<8;++i) {
        threads[count]=CreateThread(NULL,0,worker,NULL,0,&id);
        CHECK(threads[count]!=NULL); if(threads[count]) ++count;
    }
    AcquireSRWLockExclusive(&lock);
    while(ready<count) if(!SleepConditionVariableSRW(&condition,&lock,10000,0)) { ++failures; break; }
    start=TRUE; WakeAllConditionVariable(&condition);
    while(finished<count) if(!SleepConditionVariableSRW(&condition,&lock,10000,0)) { ++failures; break; }
    ReleaseSRWLockExclusive(&lock);
    for(i=0;i<count;++i) {
        CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(threads[i],&code) && code==0); CloseHandle(threads[i]);
    }
    CHECK(counter==count*1000 && initialized==1);
    context=NULL; pending=TRUE;
    CHECK(InitOnceBeginInitialize(&once,INIT_ONCE_CHECK_ONLY,&pending,&context) && !pending && context==&published);
    CHECK(!InitOnceBeginInitialize(&other,INIT_ONCE_CHECK_ONLY,&pending,NULL));
    CHECK(!InitOnceExecuteOnce(&other,retry,&calls,&context) && GetLastError()==ERROR_RETRY);
    CHECK(InitOnceExecuteOnce(&other,retry,&calls,&context) && calls==2 && context==(void *)0x1000);
    CHECK(InitOnceExecuteOnce(&other,retry,&calls,&context) && calls==2);
    InitOnceInitialize(&other);
    CHECK(InitOnceBeginInitialize(&other,INIT_ONCE_ASYNC,&pending,NULL) && pending);
    CHECK(InitOnceBeginInitialize(&other,INIT_ONCE_ASYNC,&pending,NULL) && pending);
    CHECK(!InitOnceComplete(&other,INIT_ONCE_ASYNC,(void *)1) && GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(InitOnceComplete(&other,INIT_ONCE_ASYNC,(void *)0x1000));
    CHECK(!InitOnceComplete(&other,INIT_ONCE_ASYNC,NULL));
    CHECK(InitOnceBeginInitialize(&other,0,&pending,&context) && !pending && context==(void *)0x1000);
    thread=CreateThread(NULL,0,exchange,NULL,0,&id); CHECK(thread!=NULL);
    if(thread) {
        AcquireSRWLockExclusive(&lock);
        for(i=1;i<=500;++i) {
            request=i; WakeConditionVariable(&condition);
            while(response<i) if(!SleepConditionVariableSRW(&condition,&lock,5000,0)) { ++failures; break; }
            CHECK(response==i);
            if(response!=i) break;
        }
        ReleaseSRWLockExclusive(&lock);
        CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(thread,&code) && code==0); CloseHandle(thread);
    }
    length=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!length || length>=MAX_PATH-16) ExitProcess(2);
    last=path; for(p=path;*p;++p) if(*p=='\\') last=p+1;
    lstrcpyA(last,"SYNCTEST.TXT");
    file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) ExitProcess(3);
    wsprintfA(text,"Legacy SRW, condition-variable and once tests\r\nCHECKS=%u FAILURES=%u COUNTER=%u ONCE=%u\r\nRESULT=%s\r\n",
              checks,failures,counter,initialized,failures?"FAIL":"PASS");
    length=lstrlenA(text);
    if(!WriteFile(file,text,length,&written,NULL) || written!=length) ExitProcess(4);
    CloseHandle(file); ExitProcess(failures?1:0);
}
