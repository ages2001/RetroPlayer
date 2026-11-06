/* Adapt the 32-bit time interfaces expected by current MinGW to CRTDLL.
 * The CRT DLL owns all allocations and thread-local C runtime state.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

extern long __cdecl legacy_time(long *) __asm__("_time");
extern long __cdecl legacy_mktime(struct tm *) __asm__("_mktime");
extern struct tm *__cdecl legacy_gmtime(const long *) __asm__("_gmtime");
extern struct tm *__cdecl legacy_localtime(const long *) __asm__("_localtime");

long rp_time(long *value)
{
    return legacy_time(value);
}

long rp_mktime(struct tm *value)
{
    if (!value) { errno = EINVAL; return -1; }
    return legacy_mktime(value);
}

static int convert_time(struct tm *output, const long *value, BOOL local)
{
    struct tm *result;
    if (!output) { errno = EINVAL; return EINVAL; }
    output->tm_sec = output->tm_min = output->tm_hour = output->tm_mday = -1;
    output->tm_mon = output->tm_year = output->tm_wday = output->tm_yday = -1;
    output->tm_isdst = -1;
    if (!value || *value < 0) { errno = EINVAL; return EINVAL; }
    result = local ? legacy_localtime(value) : legacy_gmtime(value);
    if (!result) { errno = EINVAL; return EINVAL; }
    *output = *result;
    return 0;
}

int rp_gmtime(struct tm *output, const long *value)
{
    return convert_time(output, value, FALSE);
}

int rp_localtime(struct tm *output, const long *value)
{
    return convert_time(output, value, TRUE);
}

int rp_fputws(const wchar_t *text, FILE *stream)
{
    if (!text || !stream) { errno = EINVAL; return EOF; }
    while (*text)
        if (fputwc(*text++, stream) == WEOF) return EOF;
    return 0;
}

BOOL WINAPI RPEntry(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    extern BOOL rp_tls_event(DWORD);
    extern BOOL rp_env_event(DWORD);
    extern BOOL rp_gthr_event(DWORD);
    (void)module; (void)reserved;
    if (reason==DLL_THREAD_DETACH || reason==DLL_PROCESS_DETACH)
        rp_gthr_event(reason);
    if (!rp_tls_event(reason)) return FALSE;
    if (!rp_env_event(reason)) {
        if (reason==DLL_PROCESS_ATTACH) rp_tls_event(DLL_PROCESS_DETACH);
        return FALSE;
    }
    if (reason==DLL_PROCESS_ATTACH && !rp_gthr_event(reason)) {
        rp_env_event(DLL_PROCESS_DETACH); rp_tls_event(DLL_PROCESS_DETACH);
        return FALSE;
    }
    return TRUE;
}
