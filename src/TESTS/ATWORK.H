#ifndef ATWORK_H
#define ATWORK_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef WORKEREXPORT
#define WORKEREXPORT static
#endif
typedef struct { unsigned a, b, c; } TRIPLE;
typedef struct {
    unsigned count, cas, errors;
    unsigned long long wide, pattern;
    TRIPLE triple;
} SHARED;

WORKEREXPORT DWORD WINAPI Worker(void *data)
{
    SHARED *s = data;
    unsigned i, expected;
    unsigned long long v;
    TRIPLE t, got;
    for (i = 0; i < 10000; ++i) {
#ifdef LEGACY_SYNC
        __sync_fetch_and_add(&s->count,1);
        expected=__atomic_load_n(&s->cas,__ATOMIC_ACQUIRE);
        for (;;) {
            unsigned old=__sync_val_compare_and_swap(&s->cas,expected,expected+1);
            if (old==expected) break;
            expected=old;
        }
        __sync_fetch_and_add(&s->wide,1);
#else
        __atomic_fetch_add(&s->count, 1, __ATOMIC_SEQ_CST);
        expected = __atomic_load_n(&s->cas, __ATOMIC_ACQUIRE);
        while (!__atomic_compare_exchange_n(&s->cas, &expected, expected+1,
                    true, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {}
        __atomic_fetch_add(&s->wide, 1, __ATOMIC_RELAXED);
#endif
        __atomic_store_n(&s->pattern, (i & 1) ? 0x0123456789abcdefULL :
                          0xfedcba9876543210ULL, __ATOMIC_RELEASE);
        v = __atomic_load_n(&s->pattern, __ATOMIC_ACQUIRE);
        if (v != 0x0123456789abcdefULL && v != 0xfedcba9876543210ULL)
            __atomic_fetch_add(&s->errors, 1, __ATOMIC_RELAXED);
        t.a = i; t.b = ~i; t.c = 0x12345678;
        __atomic_store(&s->triple, &t, __ATOMIC_RELEASE);
        __atomic_load(&s->triple, &got, __ATOMIC_ACQUIRE);
        if (got.b != ~got.a || got.c != 0x12345678)
            __atomic_fetch_add(&s->errors, 1, __ATOMIC_RELAXED);
    }
    return 0;
}
#endif
