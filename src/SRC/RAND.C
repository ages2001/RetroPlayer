/* Optional OS entropy for the modern C runtime ABI. Never substitute rand()
 * or timer values when an OS cryptographic provider is unavailable. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <errno.h>

int rp_rand_s(unsigned int *value)
{
    HMODULE module;
    BOOLEAN (WINAPI *rtl_random)(void *,ULONG);
    BOOL (WINAPI *acquire)(HCRYPTPROV *,LPCSTR,LPCSTR,DWORD,DWORD);
    BOOL (WINAPI *generate)(HCRYPTPROV,DWORD,BYTE *);
    BOOL (WINAPI *release)(HCRYPTPROV,DWORD);
    HCRYPTPROV provider;
    unsigned int result=0;
    BOOL success=FALSE;
    if (!value) { errno=EINVAL; return EINVAL; }
    *value=0;
    module=LoadLibraryA("ADVAPI32.DLL");
    if (!module) { errno=ENOSYS; return ENOSYS; }
    *(FARPROC *)&rtl_random=GetProcAddress(module,"SystemFunction036");
    if (rtl_random) success=rtl_random(&result,sizeof(result));
    if (!success) {
        *(FARPROC *)&acquire=GetProcAddress(module,"CryptAcquireContextA");
        *(FARPROC *)&generate=GetProcAddress(module,"CryptGenRandom");
        *(FARPROC *)&release=GetProcAddress(module,"CryptReleaseContext");
        if (acquire && generate && release &&
            acquire(&provider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) {
            success=generate(provider,sizeof(result),(BYTE *)&result);
            release(provider,0);
        }
    }
    FreeLibrary(module);
    if (!success) { errno=ENOSYS; return ENOSYS; }
    *value=result;
    return 0;
}
