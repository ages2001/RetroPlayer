/* UTF-8 conversion for systems predating code page 65001 support.
 * Other code pages are handled by the native ACP/code-page implementation.
 * Count first, then write: invalid input and small buffers never partially
 * modify the caller's output. UTF-16 surrogate pairs are supported explicitly.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>

static int decode(const unsigned char *s, int n, unsigned long *point, BOOL *valid)
{
    unsigned long cp;
    int length, i;
    unsigned char first = s[0];
    *valid = FALSE;
    *point = 0xfffd;
    if (first < 0x80) { *point = first; *valid = TRUE; return 1; }
    if (first >= 0xc2 && first <= 0xdf) { length = 2; cp = first & 31; }
    else if (first >= 0xe0 && first <= 0xef) { length = 3; cp = first & 15; }
    else if (first >= 0xf0 && first <= 0xf4) { length = 4; cp = first & 7; }
    else return 1;
    for (i = 1; i < length; ++i) {
        unsigned char byte;
        if (i >= n) return i;
        byte = s[i];
        if (byte < 0x80 || byte > 0xbf) return i;
        if (i == 1 && ((first == 0xe0 && byte < 0xa0) ||
            (first == 0xed && byte > 0x9f) || (first == 0xf0 && byte < 0x90) ||
            (first == 0xf4 && byte > 0x8f))) return 1;
        cp = (cp << 6) | (byte & 63);
    }
    *point = cp; *valid = TRUE;
    return length;
}

int WINAPI rp_multi_wide(UINT page, DWORD flags, LPCCH input, int bytes,
                         LPWSTR output, int capacity)
{
    int pass, used, count = 0, consumed, width;
    unsigned long cp;
    BOOL valid;
    if (page != CP_UTF8)
        return MultiByteToWideChar(page, flags, input, bytes, output, capacity);
    if (flags & ~8UL) { SetLastError(ERROR_INVALID_FLAGS); return 0; }
    if (!input || bytes == 0 || bytes < -1 || capacity < 0 ||
        (capacity && !output) || (const void *)input == (const void *)output) {
        SetLastError(ERROR_INVALID_PARAMETER); return 0;
    }
    if (bytes == -1) {
        bytes = 0;
        while (input[bytes]) {
            if (bytes == INT_MAX-1) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
            ++bytes;
        }
        ++bytes;
    }
    for (pass = 0; pass < 2; ++pass) {
        count = 0;
        for (used = 0; used < bytes; used += consumed) {
            consumed = decode((const unsigned char *)input + used, bytes-used, &cp, &valid);
            if (!valid && (flags & 8)) { SetLastError(ERROR_NO_UNICODE_TRANSLATION); return 0; }
            width = cp > 0xffff ? 2 : 1;
            if (count > INT_MAX-width) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
            if (pass) {
                if (width == 1) output[count] = (WCHAR)cp;
                else {
                    cp -= 0x10000;
                    output[count] = (WCHAR)(0xd800 | (cp >> 10));
                    output[count+1] = (WCHAR)(0xdc00 | (cp & 1023));
                }
            }
            count += width;
        }
        if (!capacity) return count;
        if (count > capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    }
    return count;
}

int WINAPI rp_wide_multi(UINT page, DWORD flags, LPCWCH input, int units,
                         LPSTR output, int capacity, LPCCH fallback, LPBOOL replaced)
{
    int pass, used, count = 0, consumed, width;
    unsigned long cp;
    if (page != CP_UTF8)
        return WideCharToMultiByte(page, flags, input, units, output, capacity, fallback, replaced);
    if (flags & ~0x80UL) { SetLastError(ERROR_INVALID_FLAGS); return 0; }
    if (!input || units == 0 || units < -1 || capacity < 0 || fallback || replaced ||
        (capacity && !output) || (const void *)input == (const void *)output) {
        SetLastError(ERROR_INVALID_PARAMETER); return 0;
    }
    if (units == -1) {
        units = 0;
        while (input[units]) {
            if (units == INT_MAX-1) { SetLastError(ERROR_INVALID_PARAMETER); return 0; }
            ++units;
        }
        ++units;
    }
    for (pass = 0; pass < 2; ++pass) {
        count = 0;
        for (used = 0; used < units; used += consumed) {
            cp = input[used]; consumed = 1;
            if (cp >= 0xd800 && cp <= 0xdbff && used+1 < units &&
                    input[used+1] >= 0xdc00 && input[used+1] <= 0xdfff) {
                cp = 0x10000 + ((cp-0xd800) << 10) + input[used+1]-0xdc00;
                consumed = 2;
            } else if (cp >= 0xd800 && cp <= 0xdfff) {
                if (flags & 0x80) { SetLastError(ERROR_NO_UNICODE_TRANSLATION); return 0; }
                cp = 0xfffd;
            }
            width = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (count > INT_MAX-width) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
            if (pass) {
                if (width == 1) output[count] = (char)cp;
                else {
                    int index;
                    if (width == 2) output[count] = (char)(0xc0 | (cp >> 6));
                    else if (width == 3) output[count] = (char)(0xe0 | (cp >> 12));
                    else output[count] = (char)(0xf0 | (cp >> 18));
                    for (index = width-1; index > 0; --index) {
                        output[count+index] = (char)(0x80 | (cp & 63));
                        cp >>= 6;
                    }
                }
            }
            count += width;
        }
        if (!capacity) return count;
        if (count > capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    }
    return count;
}
