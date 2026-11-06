/* Shared atomic ABI for the legacy MinGW runtime. Compile with
 * -fno-inline-atomics and link every consumer to RPATOM.DLL. */
#ifndef RP_386_H
#define RP_386_H
#include <windows.h>
static inline void *rp_compare_pointer(void *volatile *target, void *value, void *expected)
{
    __atomic_compare_exchange_n(target, &expected, value, 0,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
static inline LONG rp_compare_long(volatile LONG *target, LONG value, LONG expected)
{
    __atomic_compare_exchange_n(target, &expected, value, 0,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
#undef InterlockedCompareExchangePointer
#define InterlockedCompareExchangePointer(t,v,e) rp_compare_pointer((void *volatile *)(t),(v),(e))
#undef InterlockedExchangePointer
#define InterlockedExchangePointer(t,v) __atomic_exchange_n((void *volatile *)(t),(v),__ATOMIC_SEQ_CST)
#undef InterlockedCompareExchange
#define InterlockedCompareExchange(t,v,e) rp_compare_long((t),(v),(e))
#undef InterlockedExchange
#define InterlockedExchange(t,v) __atomic_exchange_n((t),(v),__ATOMIC_SEQ_CST)
#endif
