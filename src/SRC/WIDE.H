/* Lossless ACP conversion for 9x. Reject names that would address a different
 * file after best-fit conversion. NT continues to use native Unicode APIs. */
#ifndef RP_WIDE_H
#define RP_WIDE_H
static char *rp_narrow_cp(LPCWSTR wide,UINT codepage)
{
    int bytes, chars, i;
    BOOL replaced = FALSE;
    char *ansi;
    WCHAR *roundtrip;
    DWORD error;
    if (!wide) { SetLastError(ERROR_INVALID_PARAMETER); return NULL; }
    bytes = WideCharToMultiByte(codepage, 0, wide, -1, NULL, 0, NULL, &replaced);
    if (!bytes) return NULL;
    ansi = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)bytes);
    if (!ansi) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    if (!WideCharToMultiByte(codepage, 0, wide, -1, ansi, bytes, NULL, &replaced))
        goto failure;
    if (replaced) { SetLastError(ERROR_NO_UNICODE_TRANSLATION); goto failure; }
    chars = MultiByteToWideChar(codepage, 0, ansi, -1, NULL, 0);
    if (!chars) goto failure;
    roundtrip = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)chars * sizeof(WCHAR));
    if (!roundtrip) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); goto failure; }
    if (!MultiByteToWideChar(codepage, 0, ansi, -1, roundtrip, chars)) {
        error = GetLastError(); HeapFree(GetProcessHeap(), 0, roundtrip);
        SetLastError(error); goto failure;
    }
    for (i = 0; i < chars && wide[i] == roundtrip[i]; ++i) {}
    HeapFree(GetProcessHeap(), 0, roundtrip);
    if (i != chars) { SetLastError(ERROR_NO_UNICODE_TRANSLATION); goto failure; }
    return ansi;
failure:
    error = GetLastError();
    HeapFree(GetProcessHeap(), 0, ansi);
    SetLastError(error);
    return NULL;
}

static inline char *rp_narrow(LPCWSTR wide) { return rp_narrow_cp(wide,CP_ACP); }

static void rp_free_name(char *name, DWORD error)
{
    if (name) HeapFree(GetProcessHeap(), 0, name);
    SetLastError(error);
}
#endif
