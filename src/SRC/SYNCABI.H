/* GCC 14's i386 expansion drops a 64-bit __sync_lock_release completely.
 * Its narrow inline xchg/store paths also bypass our shared atomic gate.
 * Route legacy lock builtins through the out-of-line __atomic ABI instead.
 * All consumers must also compile with -fno-inline-atomics. */
#ifndef RP_SYNC_ABI_H
#define RP_SYNC_ABI_H
#define __sync_lock_test_and_set(p,v,...) __atomic_exchange_n((p),(v),__ATOMIC_SEQ_CST)
#define __sync_lock_release(p,...) __atomic_store_n((p),0,__ATOMIC_SEQ_CST)
#define __sync_synchronize(...) __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif
