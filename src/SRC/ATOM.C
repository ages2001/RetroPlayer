/* Shared i386 atomic fallback for GCC's libatomic ABI.
 * Every participating module MUST use -fno-inline-atomics and this same DLL.
 * Mixing unlocked inline stores with the locked RMW operations is invalid.
 * All memory orders are conservatively implemented as sequentially consistent.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

static volatile unsigned int gate;

static void acquire(void)
{
    unsigned int prior;
    do {
        prior = 1;
        __asm__ volatile ("xchgl %0, %1" : "+r"(prior), "+m"(gate) : : "memory");
        /* Before Server 2003, Sleep(0) yields only to equal-priority threads.
         * An audio thread can preempt the lock owner and starve it forever. */
        if (prior) Sleep(1);
    } while (prior);
}

static void release(void)
{
    unsigned int value = 0;
    __asm__ volatile ("xchgl %0, %1" : "+r"(value), "+m"(gate) : : "memory");
}

#define SCALAR(N, T) \
T rp_load##N(const volatile T *p, int order) \
{ T result; (void)order; acquire(); result = *p; release(); return result; } \
void rp_store##N(volatile T *p, T value, int order) \
{ (void)order; acquire(); *p = value; release(); } \
T rp_exchange##N(volatile T *p, T value, int order) \
{ T old; (void)order; acquire(); old = *p; *p = value; release(); return old; } \
bool rp_compare##N(volatile T *p, T *expected, T value, int success, int failure) \
{ T old; bool ok; (void)success; (void)failure; acquire(); old = *p; \
  ok = (old == *expected); if (ok) *p = value; else *expected = old; release(); return ok; }

#define FETCH(N, T, NAME, EXPR) \
T rp_fetch_##NAME##N(volatile T *p, T value, int order) \
{ T old; (void)order; acquire(); old = *p; *p = (T)(EXPR); release(); return old; } \
T rp_##NAME##_fetch##N(volatile T *p, T value, int order) \
{ T old, result; (void)order; acquire(); old = *p; result = (T)(EXPR); *p = result; \
  release(); return result; }

#define WIDTH(N, T) \
SCALAR(N, T) \
FETCH(N, T, add, old + value) \
FETCH(N, T, sub, old - value) \
FETCH(N, T, and, old & value) \
FETCH(N, T, or, old | value) \
FETCH(N, T, xor, old ^ value) \
FETCH(N, T, nand, ~(old & value))

WIDTH(1, unsigned char)
WIDTH(2, unsigned short)
WIDTH(4, unsigned int)
WIDTH(8, unsigned long long)

/* Older GCC __sync entry points have different signatures from __atomic.
 * Route both ABIs through the same gate, including cross-DLL mixed callers. */
#define SYNC_FETCH(N,T,OP) \
T rp_sync_fetch_##OP##N(volatile T *p,T v) { return rp_fetch_##OP##N(p,v,5); } \
T rp_sync_##OP##_fetch##N(volatile T *p,T v) { return rp_##OP##_fetch##N(p,v,5); }
#define SYNC_WIDTH(N,T) \
SYNC_FETCH(N,T,add) SYNC_FETCH(N,T,sub) SYNC_FETCH(N,T,and) \
SYNC_FETCH(N,T,or) SYNC_FETCH(N,T,xor) SYNC_FETCH(N,T,nand) \
T rp_sync_val_compare##N(volatile T *p,T expected,T value) \
{ rp_compare##N(p,&expected,value,5,5); return expected; } \
bool rp_sync_bool_compare##N(volatile T *p,T expected,T value) \
{ return rp_compare##N(p,&expected,value,5,5); } \
T rp_sync_lock##N(volatile T *p,T value) { return rp_exchange##N(p,value,5); } \
void rp_sync_unlock##N(volatile T *p) { rp_store##N(p,0,5); }
SYNC_WIDTH(1,unsigned char)
SYNC_WIDTH(2,unsigned short)
SYNC_WIDTH(4,unsigned int)
SYNC_WIDTH(8,unsigned long long)
void rp_sync_barrier(void) { acquire(); release(); }

void rp_load(size_t n, const volatile void *p, void *out, int order)
{
    const volatile unsigned char *src = p;
    unsigned char *dest = out;
    (void)order;
    acquire();
    while (n--) *dest++ = *src++;
    release();
}

void rp_store(size_t n, volatile void *p, const void *value, int order)
{
    volatile unsigned char *dest = p;
    const unsigned char *src = value;
    (void)order;
    acquire();
    while (n--) *dest++ = *src++;
    release();
}

void rp_exchange(size_t n, volatile void *p, const void *value, void *out, int order)
{
    volatile unsigned char *dest = p;
    const unsigned char *src = value;
    unsigned char *result = out, old;
    (void)order;
    acquire();
    while (n--) { old = *dest; *dest++ = *src++; *result++ = old; }
    release();
}

bool rp_compare(size_t n, volatile void *p, void *expected, const void *value,
                int success, int failure)
{
    volatile unsigned char *dest = p;
    const unsigned char *src = value;
    unsigned char *exp = expected;
    size_t i;
    bool ok = true;
    (void)success; (void)failure;
    acquire();
    for (i = 0; i < n; ++i) if (dest[i] != exp[i]) { ok = false; break; }
    if (ok) for (i = 0; i < n; ++i) dest[i] = src[i];
    else for (i = 0; i < n; ++i) exp[i] = dest[i];
    release();
    return ok;
}

bool rp_lock_free(size_t size, const volatile void *ptr)
{ (void)size; (void)ptr; return false; }

bool rp_test_set(volatile void *p, int order)
{ return rp_exchange1(p, 1, order) != 0; }

void rp_clear(volatile void *p, int order)
{ rp_store1(p, 0, order); }

void rp_fence(int order)
{ (void)order; acquire(); release(); }

BOOL WINAPI RPEntry(HINSTANCE module, DWORD reason, LPVOID reserved)
{ (void)module; (void)reason; (void)reserved; return TRUE; }
