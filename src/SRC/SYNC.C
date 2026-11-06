/* Vista synchronization ABI on original Win32 primitives, i386 baseline.
 * SRW/CV objects own no persistent handles. A CV waiter owns a temporary
 * event; queue removal and signaling share a gate to close timeout races. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef struct { uintptr_t value; } RP_LOCK;
typedef struct waiter {
    struct waiter *next;
    HANDLE event;
    BOOL queued;
} WAITER;
typedef struct { WAITER *head; } RP_CONDITION;
typedef struct { DWORD owner; BOOL asynchronous; } ONCE_PENDING;
typedef BOOL (WINAPI *ONCE_FUNCTION)(RP_LOCK *,void *,void **);
static volatile LONG gate;

static void enter_gate(void)
{
    LONG value;
    do {
        value=1;
        __asm__ __volatile__("xchgl %0,%1" : "+r"(value),"+m"(gate) :: "memory");
        /* Let a lower-priority gate owner run on NT3.x/NT4 and Win9x too. */
        if(value) Sleep(1);
    } while(value);
}
static void leave_gate(void)
{
    __asm__ __volatile__("" ::: "memory");
    gate=0;
}
static void invalid_lock(void)
{
    RaiseException(0xc0000264UL,EXCEPTION_NONCONTINUABLE,0,NULL);
    ExitProcess(0xc0000264UL);
}
void WINAPI rp_srw_init(RP_LOCK *lock) { lock->value=0; }
BOOL WINAPI rp_srw_try_exclusive(RP_LOCK *lock)
{
    BOOL acquired;
    enter_gate(); acquired=lock->value==0;
    if(acquired) lock->value=1;
    leave_gate(); return acquired;
}
BOOL WINAPI rp_srw_try_shared(RP_LOCK *lock)
{
    BOOL acquired;
    enter_gate(); acquired=!(lock->value & 1);
    if(acquired) {
        if(lock->value>UINTPTR_MAX-2) { leave_gate(); invalid_lock(); }
        lock->value+=2;
    }
    leave_gate(); return acquired;
}
void WINAPI rp_srw_acquire_exclusive(RP_LOCK *lock)
{
    while(!rp_srw_try_exclusive(lock)) Sleep(1);
}
void WINAPI rp_srw_acquire_shared(RP_LOCK *lock)
{
    while(!rp_srw_try_shared(lock)) Sleep(1);
}
void WINAPI rp_srw_release_exclusive(RP_LOCK *lock)
{
    enter_gate();
    if(lock->value!=1) { leave_gate(); invalid_lock(); }
    lock->value=0; leave_gate();
}
void WINAPI rp_srw_release_shared(RP_LOCK *lock)
{
    enter_gate();
    if((lock->value & 1) || lock->value<2) { leave_gate(); invalid_lock(); }
    lock->value-=2; leave_gate();
}

void WINAPI rp_condition_init(RP_CONDITION *condition) { condition->head=NULL; }
static void wake_condition(RP_CONDITION *condition,BOOL all)
{
    WAITER *waiter;
    enter_gate();
    while((waiter=condition->head)!=NULL) {
        condition->head=waiter->next; waiter->queued=FALSE;
        if(!SetEvent(waiter->event)) { leave_gate(); invalid_lock(); }
        if(!all) break;
    }
    leave_gate();
}
void WINAPI rp_condition_wake(RP_CONDITION *condition) { wake_condition(condition,FALSE); }
void WINAPI rp_condition_all(RP_CONDITION *condition) { wake_condition(condition,TRUE); }
extern void WINAPI rp_cs_enter(LPCRITICAL_SECTION);
extern void WINAPI rp_cs_leave(LPCRITICAL_SECTION);

static BOOL sleep_condition(RP_CONDITION *condition,void *lock,DWORD timeout,unsigned kind)
{
    WAITER waiter,**link;
    DWORD result,error;
    BOOL notified;
    waiter.event=CreateEventA(NULL,FALSE,FALSE,NULL);
    if(!waiter.event) return FALSE; /* The caller's lock remains held. */
    waiter.next=NULL; waiter.queued=TRUE;
    enter_gate();
    for(link=&condition->head;*link;link=&(*link)->next) {}
    *link=&waiter;
    leave_gate();
    if(kind==2) rp_cs_leave(lock);
    else if(kind==1) rp_srw_release_shared(lock);
    else rp_srw_release_exclusive(lock);
    result=WaitForSingleObject(waiter.event,timeout); error=GetLastError();
    enter_gate();
    notified=!waiter.queued;
    if(waiter.queued) {
        for(link=&condition->head;*link && *link!=&waiter;link=&(*link)->next) {}
        if(*link) *link=waiter.next;
    }
    leave_gate();
    CloseHandle(waiter.event);
    if(kind==2) rp_cs_enter(lock);
    else if(kind==1) rp_srw_acquire_shared(lock);
    else rp_srw_acquire_exclusive(lock);
    if(result==WAIT_OBJECT_0 || (result==WAIT_TIMEOUT && notified)) return TRUE;
    SetLastError(result==WAIT_TIMEOUT ? ERROR_TIMEOUT : error);
    return FALSE;
}
BOOL WINAPI rp_condition_srw(RP_CONDITION *condition,RP_LOCK *lock,DWORD timeout,ULONG flags)
{
    if(flags & ~1UL) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    return sleep_condition(condition,lock,timeout,flags & 1);
}
BOOL WINAPI rp_condition_cs(RP_CONDITION *condition,LPCRITICAL_SECTION lock,DWORD timeout)
{
    return sleep_condition(condition,lock,timeout,2);
}

void WINAPI rp_once_init(RP_LOCK *once) { once->value=0; }
BOOL WINAPI rp_once_begin(RP_LOCK *once,DWORD flags,BOOL *pending,void **context)
{
    uintptr_t state;
    ONCE_PENDING *entry;
    if(!pending || (flags & ~3UL) || flags==3) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    for(;;) {
        enter_gate(); state=once->value;
        if((state & 3)==3) {
            *pending=FALSE;
            if(context) *context=(void *)(state & ~(uintptr_t)3);
            leave_gate(); return TRUE;
        }
        if(flags & 1) { leave_gate(); SetLastError(ERROR_GEN_FAILURE); return FALSE; }
        if(!state) {
            entry=HeapAlloc(GetProcessHeap(),0,sizeof(*entry));
            if(!entry) { leave_gate(); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
            entry->owner=GetCurrentThreadId(); entry->asynchronous=(flags & 2)!=0;
            once->value=(uintptr_t)entry | (entry->asynchronous ? 2 : 1);
            *pending=TRUE; if(context) *context=NULL;
            leave_gate(); return TRUE;
        }
        if(((state & 3)==2)!=((flags & 2)!=0)) {
            leave_gate(); SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
        }
        if(flags & 2) {
            *pending=TRUE; if(context) *context=NULL;
            leave_gate(); return TRUE;
        }
        leave_gate(); Sleep(1);
    }
}
BOOL WINAPI rp_once_complete(RP_LOCK *once,DWORD flags,void *context)
{
    ONCE_PENDING *entry;
    uintptr_t state;
    if((flags & ~6UL) || flags==6 || ((uintptr_t)context & 3) ||
        ((flags & 4) && context)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    enter_gate(); state=once->value;
    if(!state || (state & 3)==3) { leave_gate(); SetLastError(ERROR_GEN_FAILURE); return FALSE; }
    entry=(ONCE_PENDING *)(state & ~(uintptr_t)3);
    if(entry->asynchronous!=((flags & 2)!=0) ||
        (!entry->asynchronous && entry->owner!=GetCurrentThreadId())) {
        leave_gate(); SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    once->value=(flags & 4) ? 0 : (uintptr_t)context | 3;
    leave_gate(); HeapFree(GetProcessHeap(),0,entry); return TRUE;
}
BOOL WINAPI rp_once_execute(RP_LOCK *once,ONCE_FUNCTION function,void *parameter,void **context)
{
    BOOL pending;
    void *value=NULL;
    DWORD error;
    if(!function) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if(!rp_once_begin(once,0,&pending,&value)) return FALSE;
    if(pending) {
        if(!function(once,parameter,&value)) {
            error=GetLastError(); rp_once_complete(once,4,NULL); SetLastError(error); return FALSE;
        }
        if(!rp_once_complete(once,0,value)) return FALSE;
    }
    if(context) *context=value;
    return TRUE;
}
