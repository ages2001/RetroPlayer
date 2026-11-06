/* Shared GCC Win32 gthread ABI for the CRTDLL port. Thread identifiers are
 * private descriptors, never pseudo-handles or recycled Windows handles. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

typedef struct thread_record {
    HANDLE handle,gate;
    void *(*function)(void *);
    void *argument,*result;
    unsigned refs;
    DWORD id;
} THREAD;
typedef struct key_record {
    struct key_record *next;
    DWORD key;
    uint64_t generation;
    void (*destroy)(void *);
} KEY;
typedef struct { int done; long started; } ONCE;
static DWORD self_key=TLS_OUT_OF_INDEXES;
static HANDLE lock;
static KEY *keys;
static uint64_t key_generation;

static void enter(void) { WaitForSingleObject(lock,INFINITE); }
static void leave(void) { ReleaseMutex(lock); }
static void drop(THREAD *thread)
{
    BOOL last;
    enter(); last=--thread->refs==0; leave();
    if (last) {
        if (thread->handle) CloseHandle(thread->handle);
        HeapFree(GetProcessHeap(),0,thread);
    }
}
static void clean_keys(void)
{
    /* Snapshot before invoking user callbacks, which can delete/create keys.
     * Repeat so a destructor can publish another thread-local value. */
    for (unsigned pass=0;pass<4;pass++) {
        KEY *k,*snapshot;
        unsigned count=0,i=0,called=0;
        enter();
        for (k=keys;k;k=k->next) ++count;
        snapshot=count ? HeapAlloc(GetProcessHeap(),0,count*sizeof(*k)) : NULL;
        if (snapshot) for (k=keys;k;k=k->next) snapshot[i++]=*k;
        leave();
        if (!snapshot) break;
        for (i=0;i<count;i++) {
            void *value=NULL;
            enter();
            for (k=keys;k;k=k->next)
                if (k->key==snapshot[i].key && k->generation==snapshot[i].generation) {
                    value=TlsGetValue(k->key);
                    if (value && k->destroy) TlsSetValue(k->key,NULL);
                    break;
                }
            leave();
            if (value && snapshot[i].destroy) {
                snapshot[i].destroy(value); ++called;
            }
        }
        HeapFree(GetProcessHeap(),0,snapshot);
        if (!called) break;
    }
}
static void __cdecl thread_start(void *argument)
{
    THREAD *thread=argument;
    WaitForSingleObject(thread->gate,INFINITE);
    CloseHandle(thread->gate); thread->gate=NULL;
    if (!TlsSetValue(self_key,thread)) ExitProcess(ERROR_NOT_ENOUGH_MEMORY);
    if (thread->function) thread->result=thread->function(thread->argument);
    /* The DLL detach notification runs key destructors before freeing emutls.
     * CRTDLL _endthread also releases the C runtime's per-thread allocation. */
    _endthread();
}
int rp_gthr_create(THREAD **out,void *(*function)(void *),void *argument)
{
    THREAD *thread;
    uintptr_t original;
    HANDLE process=GetCurrentProcess();
    if (!out || !function) return EINVAL;
    thread=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*thread));
    if (!thread) return ENOMEM;
    thread->gate=CreateEventA(NULL,FALSE,FALSE,NULL);
    if (!thread->gate) { HeapFree(GetProcessHeap(),0,thread); return EAGAIN; }
    thread->refs=2; thread->function=function; thread->argument=argument;
    original=_beginthread(thread_start,0,thread);
    if (original==(uintptr_t)-1) {
        CloseHandle(thread->gate); HeapFree(GetProcessHeap(),0,thread); return EAGAIN;
    }
    /* The new thread cannot exit until its stable join handle is duplicated. */
    if (!DuplicateHandle(process,(HANDLE)original,process,&thread->handle,0,FALSE,DUPLICATE_SAME_ACCESS)) {
        thread->function=NULL; thread->refs=1;
        SetEvent(thread->gate); return EAGAIN;
    }
    *out=thread;
    SetEvent(thread->gate);
    return 0;
}
THREAD *rp_gthr_self(void)
{
    THREAD *thread=TlsGetValue(self_key);
    if (!thread) {
        thread=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*thread));
        if (!thread) ExitProcess(ERROR_NOT_ENOUGH_MEMORY);
        thread->refs=1; thread->id=GetCurrentThreadId();
        if (!TlsSetValue(self_key,thread)) ExitProcess(ERROR_NOT_ENOUGH_MEMORY);
    }
    return thread;
}
int rp_gthr_join(THREAD *thread,void **result)
{
    if (!thread || !thread->handle) return EINVAL;
    if (thread==TlsGetValue(self_key)) return EDEADLK;
    if (WaitForSingleObject(thread->handle,INFINITE)!=WAIT_OBJECT_0) return EINVAL;
    if (result) *result=thread->result;
    drop(thread); return 0;
}
int rp_gthr_detach(THREAD *thread) { if (!thread || !thread->handle) return EINVAL; drop(thread); return 0; }
int rp_gthr_equal(THREAD *a,THREAD *b) { return a==b; }
int rp_gthr_yield(void) { Sleep(0); return 0; }
int rp_gthr_once_begin(ONCE *once)
{
    for (;;) {
        enter();
        if (once->done) { leave(); return 0; }
        if (once->started==-1) {
            once->started=0; leave(); return 1;
        }
        leave(); Sleep(1);
    }
}
void rp_gthr_once_complete(ONCE *once) { enter(); once->done=1; leave(); }
int rp_gthr_once(ONCE *once,void (*function)(void))
{ if (rp_gthr_once_begin(once)) { function(); rp_gthr_once_complete(once); } return 0; }
void rp_gthr_once_reset(ONCE *once)
{ enter(); once->started=-1; once->done=0; leave(); }
int rp_gthr_key_create(DWORD *out,void (*destroy)(void *))
{
    KEY *key=HeapAlloc(GetProcessHeap(),0,sizeof(*key));
    if (!key) return ENOMEM;
    key->key=TlsAlloc(); key->destroy=destroy;
    if (key->key==TLS_OUT_OF_INDEXES) { HeapFree(GetProcessHeap(),0,key); return EAGAIN; }
    enter(); key->generation=++key_generation; key->next=keys; keys=key; leave();
    *out=key->key; return 0;
}
int rp_gthr_key_delete(DWORD index)
{
    KEY **at,*old;
    enter();
    for (at=&keys;*at && (*at)->key!=index;at=&(*at)->next) {}
    old=*at; if (old) *at=old->next;
    leave();
    if (!old) return EINVAL;
    TlsFree(index); HeapFree(GetProcessHeap(),0,old); return 0;
}
void *rp_gthr_getspecific(DWORD key)
{ DWORD error=GetLastError(); void *value=TlsGetValue(key); SetLastError(error); return value; }
int rp_gthr_setspecific(DWORD key,const void *value)
{ return TlsSetValue(key,(void *)value) ? 0 : EINVAL; }
void rp_gthr_mutex_init(CRITICAL_SECTION *cs) { InitializeCriticalSection(cs); }
void rp_gthr_mutex_destroy(CRITICAL_SECTION *cs) { DeleteCriticalSection(cs); }
int rp_gthr_mutex_lock(CRITICAL_SECTION *cs) { EnterCriticalSection(cs); return 0; }
int rp_gthr_mutex_unlock(CRITICAL_SECTION *cs) { LeaveCriticalSection(cs); return 0; }
int rp_gthr_mutex_try(CRITICAL_SECTION *cs) { return TryEnterCriticalSection(cs) ? 0 : EBUSY; }
void rp_gthr_cond_init(CONDITION_VARIABLE *cv) { InitializeConditionVariable(cv); }
int rp_gthr_cond_signal(CONDITION_VARIABLE *cv) { WakeConditionVariable(cv); return 0; }
int rp_gthr_cond_broadcast(CONDITION_VARIABLE *cv) { WakeAllConditionVariable(cv); return 0; }
int rp_gthr_cond_wait(CONDITION_VARIABLE *cv,CRITICAL_SECTION *cs)
{ return SleepConditionVariableCS(cv,cs,INFINITE) ? 0 : EINVAL; }
int rp_gthr_cond_timedwait(CONDITION_VARIABLE *cv,CRITICAL_SECTION *cs,const struct timespec *limit)
{
    union { FILETIME ft; uint64_t value; } now;
    uint64_t target,delay;
    DWORD wait;
    if (!limit || limit->tv_nsec<0 || limit->tv_nsec>=1000000000L) return EINVAL;
    GetSystemTimeAsFileTime(&now.ft);
    now.value-=116444736000000000ULL;
    if (limit->tv_sec<0) delay=0;
    else {
        if ((uint64_t)limit->tv_sec>UINT64_MAX/10000000ULL) delay=UINT64_MAX;
        else {
            target=(uint64_t)limit->tv_sec*10000000ULL+(limit->tv_nsec+99)/100;
            delay=target>now.value ? target-now.value : 0;
        }
    }
    wait=delay>=(uint64_t)(INFINITE-1)*10000 ? INFINITE-1 : (DWORD)((delay+9999)/10000);
    if (SleepConditionVariableCS(cv,cs,wait)) return 0;
    return GetLastError()==ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;
}
BOOL rp_gthr_event(DWORD reason)
{
    if (reason==DLL_PROCESS_ATTACH) {
        self_key=TlsAlloc();
        if (self_key==TLS_OUT_OF_INDEXES) return FALSE;
        lock=CreateMutexA(NULL,FALSE,NULL);
        if (!lock) { TlsFree(self_key); self_key=TLS_OUT_OF_INDEXES; return FALSE; }
    } else if ((reason==DLL_THREAD_DETACH || reason==DLL_PROCESS_DETACH) && self_key!=TLS_OUT_OF_INDEXES) {
        THREAD *thread;
        clean_keys();
        thread=TlsGetValue(self_key); TlsSetValue(self_key,NULL);
        if (thread) drop(thread);
        if (reason==DLL_PROCESS_DETACH) {
            KEY *key=keys;
            while (key) { KEY *next=key->next; TlsFree(key->key); HeapFree(GetProcessHeap(),0,key); key=next; }
            keys=NULL; CloseHandle(lock); TlsFree(self_key); self_key=TLS_OUT_OF_INDEXES;
        }
    }
    return TRUE;
}
