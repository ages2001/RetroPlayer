/* Legacy module reference acquisition used by C++ thread-local destructors.
 * Callers must keep code addresses valid during registration, as for dlsym. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef GET_MODULE_HANDLE_EX_FLAG_PIN
#define GET_MODULE_HANDLE_EX_FLAG_PIN 1
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#endif
extern HMODULE WINAPI rp_module_handle(LPCWSTR);
extern DWORD WINAPI rp_module_name(HMODULE,LPWSTR,DWORD);
extern HMODULE WINAPI rp_load(LPCWSTR);

BOOL WINAPI rp_module_ex(DWORD flags,LPCWSTR name,HMODULE *out)
{
    HMODULE module,loaded;
    WCHAR path[MAX_PATH];
    DWORD count;
    MEMORY_BASIC_INFORMATION info;
    if (!out || (flags&~7UL) || (flags&3)==3) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    *out=NULL;
    /* These kernels cannot make a DLL permanently non-unloadable. */
    if (flags&GET_MODULE_HANDLE_EX_FLAG_PIN) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    if (flags&GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) {
        if (!VirtualQuery(name,&info,sizeof(info))) return FALSE;
        module=(HMODULE)info.AllocationBase;
        if (!module) { SetLastError(ERROR_MOD_NOT_FOUND); return FALSE; }
        /* GetModuleFileName rejects allocation bases that are not modules. */
    } else {
        module=rp_module_handle(name);
        if (!module) return FALSE;
    }
    count=rp_module_name(module,path,MAX_PATH);
    if (!count || count>=MAX_PATH) { SetLastError(ERROR_MOD_NOT_FOUND); return FALSE; }
    if (flags&GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) { *out=module; return TRUE; }
    /* The executable itself cannot be unloaded through FreeLibrary. */
    if (module==GetModuleHandleA(NULL)) { *out=module; return TRUE; }
    loaded=rp_load(path);
    if (!loaded) return FALSE;
    if (loaded!=module) { FreeLibrary(loaded); SetLastError(ERROR_MOD_NOT_FOUND); return FALSE; }
    *out=loaded; return TRUE;
}
