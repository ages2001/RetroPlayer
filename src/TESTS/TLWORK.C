/* Independently linked DLL: its TLS must remain distinct from the caller. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static __thread unsigned value = 23;
__declspec(dllexport) unsigned __cdecl tls_work(unsigned expected, unsigned next)
{
    unsigned result = value == expected;
    value = next;
    return result;
}
BOOL WINAPI RPEntry(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}
