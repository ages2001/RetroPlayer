#include <stdbool.h>
#include "ATWORK.H"

static SHARED shared;
static unsigned failures, checks, first_failure;
static char directory[MAX_PATH];

#define CHECK(expression) do { ++checks; if (!(expression)) { \
    ++failures; if (!first_failure) first_failure = checks; } } while (0)
#define TEST_WIDTH(T) do { \
    T p = 7, expected = 6; \
    CHECK(__atomic_load_n(&p, __ATOMIC_RELAXED) == 7); \
    __atomic_store_n(&p, (T)9, __ATOMIC_RELEASE); CHECK(p == 9); \
    CHECK(__atomic_exchange_n(&p, (T)4, __ATOMIC_ACQ_REL) == 9 && p == 4); \
    CHECK(!__atomic_compare_exchange_n(&p, &expected, (T)10, false, \
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && expected == 4 && p == 4); \
    CHECK(__atomic_compare_exchange_n(&p, &expected, (T)10, true, \
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && p == 10); \
    CHECK(__atomic_fetch_add(&p, (T)3, __ATOMIC_RELAXED) == 10 && p == 13); \
    CHECK(__atomic_sub_fetch(&p, (T)2, __ATOMIC_RELAXED) == 11 && p == 11); \
    CHECK(__atomic_fetch_and(&p, (T)3, __ATOMIC_RELAXED) == 11 && p == 3); \
    CHECK(__atomic_or_fetch(&p, (T)8, __ATOMIC_RELAXED) == 11 && p == 11); \
    CHECK(__atomic_fetch_xor(&p, (T)1, __ATOMIC_RELAXED) == 11 && p == 10); \
    CHECK(__atomic_nand_fetch(&p, (T)15, __ATOMIC_RELAXED) == (T)~(T)10); \
    __atomic_store_n(&p, (T)~(T)0, __ATOMIC_RELAXED); \
    CHECK(__atomic_add_fetch(&p, (T)1, __ATOMIC_RELAXED) == 0); \
    CHECK(__sync_add_and_fetch(&p,(T)4)==4); \
    CHECK(__sync_fetch_and_sub(&p,(T)1)==4 && p==3); \
    CHECK(__sync_val_compare_and_swap(&p,(T)2,(T)8)==3 && p==3); \
    CHECK(__sync_bool_compare_and_swap(&p,(T)3,(T)8) && p==8); \
    CHECK(__sync_lock_test_and_set(&p,(T)1)==8 && p==1); \
    __sync_lock_release(&p); CHECK(p==0); \
    CHECK(__sync_fetch_and_or(&p,(T)15)==0 && p==15); \
    CHECK(__sync_and_and_fetch(&p,(T)3)==3); \
    CHECK(__sync_xor_and_fetch(&p,(T)1)==2); \
    CHECK(__sync_nand_and_fetch(&p,(T)3)==(T)~(T)2); \
} while (0)

void WINAPI RPEntry(void)
{
    DWORD count, i, wait = 0xffffffffUL, exitcode, wait_error = 0, thread_id;
    HANDLE threads[4], report;
    char path[MAX_PATH], text[512], *p, *last;
    HMODULE module;
    LPTHREAD_START_ROUTINE external;
    TRIPLE value = {7,8,9}, expected = {1,2,3}, old;
    bool flag = false;
    TEST_WIDTH(unsigned char);
    TEST_WIDTH(unsigned short);
    TEST_WIDTH(unsigned int);
    TEST_WIDTH(unsigned long long);
    CHECK(!__atomic_is_lock_free(8, &shared.wide));
    CHECK(!__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST) && flag);
    __atomic_clear(&flag, __ATOMIC_SEQ_CST); CHECK(!flag);
    shared.triple = expected;
    __atomic_exchange(&shared.triple, &value, &old, __ATOMIC_SEQ_CST);
    CHECK(old.a == 1 && old.b == 2 && old.c == 3);
    CHECK(!__atomic_compare_exchange(&shared.triple, &expected, &old, false,
           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && expected.a == 7 && expected.c == 9);
    CHECK(__atomic_compare_exchange(&shared.triple, &expected, &old, false,
           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    __sync_synchronize();
    count = GetModuleFileNameA(NULL, directory, MAX_PATH);
    if (!count || count >= MAX_PATH-16) ExitProcess(2);
    last = directory;
    for (p = directory; *p; ++p) if (*p == '\\') last = p+1;
    *last = 0;
    lstrcpyA(path, directory); lstrcatA(path, "ATWORK.DLL");
    module = LoadLibraryA(path);
    external = module ? (LPTHREAD_START_ROUTINE)GetProcAddress(module, "Worker@4") : NULL;
    CHECK(external != NULL);
    shared.pattern = 0x0123456789abcdefULL;
    shared.triple.a = 0; shared.triple.b = ~0U; shared.triple.c = 0x12345678;
    if (external) {
        for (i = 0; i < 4; ++i) {
            /* NT 3.x requires a writable thread-ID output. Passing NULL
             * terminated this process with exit code 128 on both guests. */
            threads[i] = CreateThread(NULL, 0, i < 2 ? Worker : external, &shared, 0, &thread_id);
            if (!threads[i]) ExitProcess(3);
        }
        wait = WaitForMultipleObjects(4, threads, TRUE, 60000);
        if (wait == WAIT_FAILED) wait_error = GetLastError();
        /* Win32 permits the whole WAIT_OBJECT_0 .. +count-1 success range
         * even for wait-all. NT4 returns the final object's index here. */
        CHECK(wait - WAIT_OBJECT_0 < 4);
        if (wait - WAIT_OBJECT_0 < 4) {
            for (i = 0; i < 4; ++i) {
                CHECK(GetExitCodeThread(threads[i], &exitcode) && exitcode == 0);
                CloseHandle(threads[i]);
            }
            CHECK(shared.count == 40000);
            CHECK(shared.cas == 40000);
            CHECK(shared.wide == 40000);
            CHECK(shared.errors == 0);
        }
    }
    lstrcpyA(path, directory); lstrcatA(path, "ATTEST.TXT");
    report = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (report == INVALID_HANDLE_VALUE) ExitProcess(4);
    wsprintfA(text, "RPATOM GCC ABI and cross-module concurrency test\r\n"
        "CHECKS=%u FAILURES=%u FIRST=%u WAIT=%lu ERROR=%lu\r\n"
        "COUNT=%u CAS=%u WIDE_LOW=%u TORN=%u\r\nRESULT=%s\r\n",
        checks, failures, first_failure, wait, wait_error,
        shared.count, shared.cas, (unsigned)shared.wide, shared.errors,
        failures ? "FAIL" : "PASS");
    if (!WriteFile(report, text, (DWORD)lstrlenA(text), &count, NULL) || count != (DWORD)lstrlenA(text))
        ExitProcess(5);
    CloseHandle(report);
    ExitProcess(failures ? 1 : 0);
}
