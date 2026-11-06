/* Execute all compatibility tests and reject stale or missing reports. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char folder[MAX_PATH], path[MAX_PATH], command[MAX_PATH+4];
#if defined(ASM_ONLY)
static const char *programs[]={"CPUTEST", "FILETEST", "CACHE", "KTEST", "SYNCTEST", "CRTEST", "DEPTST",
                               "CORETEST", "PCMTEST", "MEDTEST", "ZTEST", "SUBTEST", "ASSVID", "SEEKTEST"};
#elif defined(TEXT_ONLY)
static const char *programs[]={"KTEST", "SYNCTEST", "CRTEST", "DEPTST", "CORETEST",
                               "PCMTEST", "MEDTEST", "ZTEST", "SUBTEST"};
#elif defined(CXX_ONLY)
static const char *programs[]={"ATTEST", "KTEST", "SYNCTEST", "CRTEST", "CXXTRY"};
#elif defined(MEDIA_ONLY)
static const char *programs[]={"KTEST", "SYNCTEST", "CRTEST", "CORETEST", "PCMTEST", "MEDTEST"};
#elif defined(CORE_ONLY)
static const char *programs[]={"CORETEST", "PCMTEST"};
#else
static const char *programs[]={"ATTEST", "KTEST", "SYNCTEST", "CRTEST", "TLTEST", "NETTEST"};
#endif
static STARTUPINFOA startup;
static PROCESS_INFORMATION process;

void WINAPI RPEntry(void)
{
    char *p, *last, text[512], data[512];
    HANDLE report, input;
    unsigned i, failures=0, blocked=0;
    DWORD length, written, wait, code, error, read, version=GetVersion();
    BOOL pass, created;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    length=GetModuleFileNameA(NULL,folder,MAX_PATH);
    if (!length || length >= MAX_PATH-20) ExitProcess(2);
    last=folder;
    for (p=folder; *p; ++p) if (*p=='\\') last=p+1;
    *last=0;
    lstrcpyA(path,folder); lstrcatA(path,"SUITE.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report==INVALID_HANDLE_VALUE) ExitProcess(3);
    wsprintfA(text,"RetroPlayer compatibility suite\r\nOS=%u.%u FAMILY=%s\r\n",
              LOBYTE(LOWORD(version)),HIBYTE(LOWORD(version)),version & 0x80000000UL ? "9x" : "NT");
    if (!WriteFile(report,text,(DWORD)lstrlenA(text),&written,NULL)) ExitProcess(4);
    for (i=0; i<sizeof(programs)/sizeof(programs[0]); ++i) {
        pass=FALSE; wait=code=0xffffffffUL; error=0;
        lstrcpyA(path,folder); lstrcatA(path,programs[i]); lstrcatA(path,".TXT");
        if (!DeleteFileA(path) && GetLastError()!=ERROR_FILE_NOT_FOUND) error=GetLastError();
        if (!error) {
            lstrcpyA(path,folder); lstrcatA(path,programs[i]); lstrcatA(path,".EXE");
            lstrcpyA(command,"\""); lstrcatA(command,path); lstrcatA(command,"\"");
            startup.cb=sizeof(startup);
            created=CreateProcessA(path,command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process);
            if (!created) error=GetLastError();
            else {
                CloseHandle(process.hThread);
                wait=WaitForSingleObject(process.hProcess,90000);
                if (wait==WAIT_TIMEOUT) {
                    TerminateProcess(process.hProcess,ERROR_TIMEOUT);
                    WaitForSingleObject(process.hProcess,5000);
                }
                if (!GetExitCodeProcess(process.hProcess,&code)) error=GetLastError();
                CloseHandle(process.hProcess);
            }
        }
        if (!error && wait==WAIT_OBJECT_0 && (code==0 || code==77)) {
            lstrcpyA(path,folder); lstrcatA(path,programs[i]); lstrcatA(path,".TXT");
            input=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if (input!=INVALID_HANDLE_VALUE) {
                unsigned matched=0,total=0,n;
                const char *match=code==77 ? "RESULT=BLOCKED\r\n" : "RESULT=PASS\r\n";
                for(;;) {
                    if(!ReadFile(input,data,sizeof(data),&read,NULL)) { pass=FALSE; break; }
                    if(!read) break;
                    total+=read;
                    if(total>1048576) { pass=FALSE; break; }
                    for(n=0;n<read;++n) {
                        if(data[n]==match[matched]) ++matched;
                        else matched=data[n]==match[0] ? 1 : 0;
                        if(!match[matched]) { pass=TRUE; matched=0; }
                    }
                }
                CloseHandle(input);
            }
        }
        if (!pass) ++failures;
        else if (code==77) ++blocked;
        wsprintfA(text,"%s WAIT=%lu EXIT=%lu ERROR=%lu RESULT=%s\r\n",
                  programs[i],wait,code,error,!pass ? "FAIL" : code==77 ? "BLOCKED" : "PASS");
        length=(DWORD)lstrlenA(text);
        if (!WriteFile(report,text,length,&written,NULL) || written!=length) ExitProcess(5);
        FlushFileBuffers(report);
    }
    wsprintfA(text,"FAILURES=%u BLOCKED=%u\r\nRESULT=%s\r\n",failures,blocked,
              failures ? "FAIL" : blocked ? "BLOCKED" : "PASS");
    length=(DWORD)lstrlenA(text);
    if (!WriteFile(report,text,length,&written,NULL) || written!=length) ExitProcess(6);
    CloseHandle(report); ExitProcess(failures ? 1 : blocked ? 77 : 0);
}
