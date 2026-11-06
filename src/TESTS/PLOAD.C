/* Locate plugin initialization/finalization failures without hiding crashes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef RP_PREFLIGHT
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "RPLOAD.H"
#endif
static char folder[MAX_PATH], pattern[MAX_PATH], path[MAX_PATH], line[512];
static WIN32_FIND_DATAA found;
static HANDLE report;
static HMODULE handles[1024];
static char names[1024][16];
static char skip[4096];
static void write_line(void)
{
    DWORD n;
    if(!WriteFile(report,line,(DWORD)lstrlenA(line),&n,NULL)) ExitProcess(3);
    FlushFileBuffers(report);
}
void WINAPI RPEntry(void)
{
    HANDLE search;
    HMODULE library;
    DWORD error;
    unsigned loaded=0,failed=0,i;
    char *p,*last;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    GetModuleFileNameA(NULL,path,MAX_PATH);last=path;
    for(p=path;*p;++p) if(*p=='\\') last=p+1;
    lstrcpyA(last,"PLOAD.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE) ExitProcess(2);
    lstrcpyA(last,"SKIP.TXT");
    {
        DWORD count;
        HANDLE input=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if(input!=INVALID_HANDLE_VALUE) {
            if(!ReadFile(input,skip,sizeof(skip)-1,&count,NULL)) ExitProcess(9);
            skip[count]=0;CloseHandle(input);
        }
    }
    lstrcpyA(folder,LOBYTE(LOWORD(GetVersion()))<4 ? "C:\\RETROPLA\\" :
             "C:\\Program Files\\RetroPlayer\\");
    if(!SetCurrentDirectoryA(folder)) ExitProcess(4);
#ifdef RP_PREFLIGHT
    {
        char temporary[MAX_PATH];wchar_t wide[MAX_PATH];DWORD count;
        unsigned char malformed[128]={0};
        if(!GetTempFileNameA(folder,"RPG",0,temporary)) ExitProcess(12);
        HANDLE bad=CreateFileA(temporary,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,0,NULL);
        if(bad==INVALID_HANDLE_VALUE) ExitProcess(12);
        /* A valid DOS signature with an NT header offset outside the file. */
        malformed[0]='M';malformed[1]='Z';malformed[60]=0xff;malformed[61]=0xff;
        BOOL written=WriteFile(bad,malformed,sizeof(malformed),&count,NULL);
        CloseHandle(bad);
        BOOL converted=MultiByteToWideChar(CP_ACP,0,temporary,-1,wide,MAX_PATH)!=0;
        BOOL rejected=written && count==sizeof(malformed) && converted && !rp_preflight(wide);
        DeleteFileA(temporary);
        if(!rejected) ExitProcess(13);
        lstrcpyA(line,"MALFORMED=REJECTED\r\n");write_line();
    }
#endif
    lstrcpyA(pattern,folder);lstrcatA(pattern,"PLUGINS\\*.DLL");
    search=FindFirstFileA(pattern,&found);
    if(search==INVALID_HANDLE_VALUE) ExitProcess(5);
    do {
        BOOL omit=FALSE;
        if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
        for(p=skip;*p;) {
            char name[16];unsigned n=0;
            while(*p && *p!='\n' && n<15) { if(*p!='\r') name[n++]=*p; ++p; }
            name[n]=0;if(*p=='\n') ++p;
            if(!lstrcmpiA(name,found.cFileName)) { omit=TRUE;break; }
        }
        if(omit) continue;
        lstrcpyA(path,folder);lstrcatA(path,"PLUGINS\\");lstrcatA(path,found.cFileName);
        wsprintfA(line,"LOAD %s\r\n",found.cFileName);write_line();
#ifdef RP_PREFLIGHT
        {
            wchar_t wide[MAX_PATH];
            if(!MultiByteToWideChar(CP_ACP,0,path,-1,wide,MAX_PATH) || !rp_preflight(wide)) {
                ++failed;wsprintfA(line,"PREFLIGHT ERROR=%lu DLL=%.200s SYMBOL=%.200s\r\n",GetLastError(),rp_last_dll,rp_last_symbol);write_line();continue;
            }
        }
#endif
        library=LoadLibraryA(path);error=GetLastError();
        if(!library) { ++failed; wsprintfA(line,"UNAVAILABLE ERROR=%lu\r\n",error);write_line();continue; }
        if(loaded>=1024) ExitProcess(8);
        if(lstrlenA(found.cFileName)>=16) ExitProcess(10);
        handles[loaded]=library;lstrcpyA(names[loaded],found.cFileName);++loaded;
        lstrcpyA(line,"LOADED\r\n");write_line();
    } while(FindNextFileA(search,&found));
    error=GetLastError();FindClose(search);
    for(i=loaded;i>0;--i) {
        wsprintfA(line,"UNLOAD %s ATOM=%08lx CORE=%08lx\r\n",names[i-1],
                  (DWORD)GetModuleHandleA("RPATOM.DLL"),(DWORD)GetModuleHandleA("LIBCORE.DLL"));write_line();
        if(!FreeLibrary(handles[i-1])) ExitProcess(6);
#ifdef RP_PREFLIGHT
        if(GetModuleHandleA("LIBCORE.DLL") && !GetModuleHandleA("RPATOM.DLL")) {
            lstrcpyA(line,"LIVE_CORE_DEPENDENCY_LOST\r\nRESULT=FAIL\r\n");write_line();ExitProcess(11);
        }
#endif
        lstrcpyA(line,"UNLOADED\r\n");write_line();
    }
    wsprintfA(line,"LOADED=%u UNAVAILABLE=%u ENUM_ERROR=%lu\r\nRESULT=%s\r\n",
              loaded,failed,error,error==ERROR_NO_MORE_FILES ? "COMPLETE" : "FAIL");
    write_line();CloseHandle(report);ExitProcess(error==ERROR_NO_MORE_FILES?0:7);
}
