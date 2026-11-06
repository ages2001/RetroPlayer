#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI RPEntry(void)
{
    char path[MAX_PATH],text[256],*last,*p;
    DWORD length,written,error=0;
    unsigned count=0,failures=1,first=0;
    unsigned (__cdecl *run)(unsigned *,unsigned *);
    HMODULE module;
    HANDLE report;
    length=GetModuleFileNameA(NULL,path,MAX_PATH);
    if (!length || length>=MAX_PATH-16) ExitProcess(2);
    last=path; for(p=path;*p;p++) if (*p=='\\') last=p+1;
    lstrcpyA(last,"CXXWORK.DLL");
    module=LoadLibraryA(path);
    if (!module) error=GetLastError();
    else {
        *(FARPROC *)&run=GetProcAddress(module,"Run");
        if (!run) error=GetLastError();
        else failures=run(&count,&first);
        FreeLibrary(module);
    }
    wsprintfA(text,"C++ runtime and thread cleanup test\r\nCHECKS=%u FAILURES=%u FIRST=%u LOAD_ERROR=%lu\r\nRESULT=%s\r\n",
        count,failures,first,error,failures ? "FAIL" : "PASS");
    lstrcpyA(last,"CXXTRY.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report==INVALID_HANDLE_VALUE) ExitProcess(3);
    length=(DWORD)lstrlenA(text);
    if (!WriteFile(report,text,length,&written,NULL) || written!=length) ExitProcess(4);
    CloseHandle(report); ExitProcess(failures ? 1 : 0);
}
