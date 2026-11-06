/* Private VM staging helper, not a release installer. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef MEDIA_UPDATE
static const char *files[]={"RETRO.EXE","LIBCORE.DLL","MEDTEST.EXE","MEDTRY.EXE"};
#define RUNNER "\\MEDTRY.EXE"
#else
static const char *files[]={"RETRO.EXE","LIBVLC.DLL","LIBCORE.DLL","RPATOM.DLL",
    "RPKERN.DLL","RPCRT.DLL","RPNET.DLL","CORETEST.EXE","PCMTEST.EXE","CORETRY.EXE"};
#define RUNNER "\\CORETRY.EXE"
#endif
static char source[MAX_PATH], target[MAX_PATH], folder[MAX_PATH], command[MAX_PATH+4];
static STARTUPINFOA startup;
static PROCESS_INFORMATION process;
static WIN32_FIND_DATAA found;
static BOOL copy_application(const char *from,const char *to)
{
    DWORD attributes=GetFileAttributesA(to);
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) &&
       !SetFileAttributesA(to,attributes & ~FILE_ATTRIBUTE_READONLY)) return FALSE;
    if(!CopyFileA(from,to,FALSE)) return FALSE;
    attributes=GetFileAttributesA(to);
    if(attributes==INVALID_FILE_ATTRIBUTES) return FALSE;
    return !(attributes & FILE_ATTRIBUTE_READONLY) ||
           SetFileAttributesA(to,attributes & ~FILE_ATTRIBUTE_READONLY);
}
void WINAPI RPEntry(void)
{
    char *p,*last,text[256];
    DWORD length,written,error=0,test_exit=0xffffffffUL,wait;
    HANDLE output;
    unsigned i;
    length=GetModuleFileNameA(NULL,source,MAX_PATH);
    if (!length || length>=MAX_PATH-16) ExitProcess(2);
    last=source;
    for (p=source; *p; ++p) if (*p=='\\') last=p+1;
    if (LOBYTE(LOWORD(GetVersion()))<4) lstrcpyA(folder,"C:\\RETROPLA");
    else {
        if (!CreateDirectoryA("C:\\Program Files",NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) error=GetLastError();
        lstrcpyA(folder,"C:\\Program Files\\RetroPlayer");
    }
    if (!error && !CreateDirectoryA(folder,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) error=GetLastError();
    for (i=0; !error && i<sizeof(files)/sizeof(files[0]); ++i) {
        lstrcpyA(last,files[i]);
        lstrcpyA(target,folder); lstrcatA(target,"\\"); lstrcatA(target,files[i]);
        if (!copy_application(source,target)) error=GetLastError();
    }
    if (!error) {
        HANDLE search;
        char source_plugin[MAX_PATH], target_plugin[MAX_PATH];
        lstrcpyA(target,folder); lstrcatA(target,"\\PLUGINS");
        if (!CreateDirectoryA(target,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
            error=GetLastError();
        lstrcpyA(last,"PLUGINS\\*.DLL");
        search=FindFirstFileA(source,&found);
        if (search==INVALID_HANDLE_VALUE) error=GetLastError();
        else {
            do {
                if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                if (lstrlenA(found.cFileName)>12) { error=ERROR_INVALID_NAME; break; }
                lstrcpyA(last,"PLUGINS\\");
                lstrcpyA(source_plugin,source); lstrcatA(source_plugin,found.cFileName);
                lstrcpyA(target_plugin,target); lstrcatA(target_plugin,"\\");
                lstrcatA(target_plugin,found.cFileName);
                if (!copy_application(source_plugin,target_plugin)) { error=GetLastError(); break; }
            } while (FindNextFileA(search,&found));
            if (!error && GetLastError()!=ERROR_NO_MORE_FILES) error=GetLastError();
            FindClose(search);
        }
    }
    if (!error) {
        lstrcpyA(target,folder); lstrcatA(target,"\\TRACE.TXT"); DeleteFileA(target);
        lstrcpyA(target,folder); lstrcatA(target,RUNNER);
        lstrcpyA(command,"\""); lstrcatA(command,target); lstrcatA(command,"\"");
        startup.cb=sizeof(startup);
        if (!CreateProcessA(target,command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process)) error=GetLastError();
        else {
            CloseHandle(process.hThread);
            wait=WaitForSingleObject(process.hProcess,110000);
            if (wait!=WAIT_OBJECT_0) error=wait==WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
            if (!GetExitCodeProcess(process.hProcess,&test_exit) && !error) error=GetLastError();
            CloseHandle(process.hProcess);
        }
#ifndef CD_UPDATE
        lstrcpyA(target,folder); lstrcatA(target,"\\CORETEST.TXT");
        lstrcpyA(last,"CORETEST.TXT");
        if (!CopyFileA(target,source,FALSE) && !error) error=GetLastError();
        lstrcpyA(target,folder); lstrcatA(target,"\\SUITE.TXT");
        lstrcpyA(last,"SUITE.TXT");
        if (!CopyFileA(target,source,FALSE) && !error) error=GetLastError();
        lstrcpyA(target,folder); lstrcatA(target,"\\TRACE.TXT");
        lstrcpyA(last,"TRACE.TXT");
        CopyFileA(target,source,FALSE);
#ifdef MEDIA_UPDATE
        {
            const char *reports[]={"MEDTEST.TXT","MEDLOG.TXT","KTEST.TXT","CRTEST.TXT"};
            for(i=0;i<sizeof(reports)/sizeof(reports[0]);++i) {
                lstrcpyA(target,folder); lstrcatA(target,"\\"); lstrcatA(target,reports[i]);
                lstrcpyA(last,reports[i]);
                if(!CopyFileA(target,source,FALSE) && !error) error=GetLastError();
            }
        }
#endif
        lstrcpyA(target,folder); lstrcatA(target,"\\PCMTEST.TXT");
        lstrcpyA(last,"PCMTEST.TXT");
        CopyFileA(target,source,FALSE);
        lstrcpyA(target,folder); lstrcatA(target,"\\PCMLOG.TXT");
        lstrcpyA(last,"PCMLOG.TXT");
        CopyFileA(target,source,FALSE);
#endif
    }
#ifdef CD_UPDATE
    lstrcpyA(source,folder); lstrcatA(source,"\\");
    last=source+lstrlenA(source);
#endif
    lstrcpyA(last,"STAGE.TXT");
    output=CreateFileA(source,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (output==INVALID_HANDLE_VALUE) ExitProcess(3);
    wsprintfA(text,"Private VM engine staging\r\nTARGET=%s\r\nERROR=%lu TEST_EXIT=%lu\r\nRESULT=%s\r\n",folder,error,test_exit,(error || test_exit) ? "FAIL" : "PASS");
    if (!WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL) || written!=(DWORD)lstrlenA(text)) ExitProcess(4);
    CloseHandle(output); ExitProcess((error || test_exit) ? 1 : 0);
}
