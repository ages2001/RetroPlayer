/* GCC emulated TLS ABI implemented with the original Win32 TLS operations.
 * No native PE TLS, FLS, CMPXCHG or C runtime allocation is required.
 * All modules must import these entry points from the same RPCRT.DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t size, alignment;
    union { uintptr_t offset; void *pointer; } location;
    void *initial;
} RP_TLS_OBJECT;

typedef struct rp_tls_value {
    struct rp_tls_value *next;
    uintptr_t identifier;
    void *value;
} RP_TLS_VALUE;

static DWORD tls_key = TLS_OUT_OF_INDEXES;
static HANDLE tls_lock;
static uintptr_t next_identifier;

static void tls_fatal(DWORD code)
{
    RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, NULL);
    ExitProcess(code);
}

static void lock_tls(void)
{
    DWORD result = WaitForSingleObject(tls_lock, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
        tls_fatal(0xc0000008UL);
}

void *rp_tls_address(RP_TLS_OBJECT *object)
{
    DWORD saved_error = GetLastError();
    RP_TLS_VALUE *head, *entry;
    uintptr_t identifier, aligned, alignment, size;
    unsigned char *out, *in;
    size_t i;
    if (!object) tls_fatal(0xc000000dUL);
    lock_tls();
    identifier = object->location.offset;
    if (!identifier) {
        if (next_identifier == UINTPTR_MAX) tls_fatal(0xc0000017UL);
        identifier = ++next_identifier;
        object->location.offset = identifier;
    }
    alignment = object->alignment;
    size = object->size;
    ReleaseMutex(tls_lock);
    head = TlsGetValue(tls_key);
    for (entry = head; entry; entry = entry->next)
        if (entry->identifier == identifier) {
            SetLastError(saved_error);
            return entry->value;
        }
    if (!alignment) alignment = 1;
    if ((alignment & (alignment - 1)) || alignment > UINTPTR_MAX - sizeof(*entry) ||
        size > UINTPTR_MAX - sizeof(*entry) - alignment)
        tls_fatal(0xc000000dUL);
    entry = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*entry) + alignment + size);
    if (!entry) tls_fatal(0xc0000017UL);
    aligned = ((uintptr_t)(entry + 1) + alignment - 1) & ~(alignment - 1);
    entry->value = (void *)aligned;
    entry->identifier = identifier;
    entry->next = head;
    out = entry->value;
    in = object->initial;
    if (in) for (i = 0; i < size; ++i) out[i] = in[i];
    if (!TlsSetValue(tls_key, entry)) {
        HeapFree(GetProcessHeap(), 0, entry);
        tls_fatal(0xc0000017UL);
    }
    SetLastError(saved_error);
    return entry->value;
}

void rp_tls_register(RP_TLS_OBJECT *object, uintptr_t size, uintptr_t alignment, void *initial)
{
    DWORD saved_error = GetLastError();
    lock_tls();
    /* Common symbols are merged before use, as required by GCC's ABI. */
    if (object->location.offset && (size > object->size || alignment > object->alignment))
        tls_fatal(0xc000000dUL);
    if (object->size < size) {
        object->size = size;
        object->initial = NULL;
    }
    if (object->alignment < alignment) object->alignment = alignment;
    if (initial && size == object->size) object->initial = initial;
    ReleaseMutex(tls_lock);
    SetLastError(saved_error);
}

BOOL rp_tls_event(DWORD reason)
{
    RP_TLS_VALUE *entry, *next;
    if (reason == DLL_PROCESS_ATTACH) {
        tls_key = TlsAlloc();
        if (tls_key == TLS_OUT_OF_INDEXES) return FALSE;
        tls_lock = CreateMutexA(NULL, FALSE, NULL);
        if (!tls_lock) { TlsFree(tls_key); tls_key = TLS_OUT_OF_INDEXES; return FALSE; }
    } else if (reason == DLL_THREAD_DETACH || reason == DLL_PROCESS_DETACH) {
        if (tls_key == TLS_OUT_OF_INDEXES) return TRUE;
        entry = TlsGetValue(tls_key);
        TlsSetValue(tls_key, NULL);
        while (entry) {
            next = entry->next;
            HeapFree(GetProcessHeap(), 0, entry);
            entry = next;
        }
        if (reason == DLL_PROCESS_DETACH) {
            TlsFree(tls_key); tls_key = TLS_OUT_OF_INDEXES;
            CloseHandle(tls_lock);
        }
    }
    return TRUE;
}
