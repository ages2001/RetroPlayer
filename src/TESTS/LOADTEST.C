#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static const char *names[]={"BASE.DLL","TLSZERO.DLL","HDRZERO.DLL","NOENTRY.DLL"};
void WINAPI RPEntry(void)
{
 char path[MAX_PATH], text[160], *p,*last;
 DWORD size,written,error;
 HANDLE output;
 unsigned i;
 HMODULE module;
 size=GetModuleFileNameA(NULL,path,MAX_PATH);
 if (!size || size>=MAX_PATH-16) ExitProcess(2);
 last=path;
 for (p=path; *p; ++p) if (*p=='\\') last=p+1;
 lstrcpyA(last,"LOADTEST.TXT");
 output=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if (output==INVALID_HANDLE_VALUE) ExitProcess(3);
 for(i=0;i<4;++i) {
  lstrcpyA(last,names[i]); SetLastError(0); module=LoadLibraryA(path);error=GetLastError();
  wsprintfA(text,"%s HANDLE=%08lx ERROR=%lu\r\n",names[i],(DWORD)module,error);
  WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); FlushFileBuffers(output);
  if(module) FreeLibrary(module);
 }
 CloseHandle(output);ExitProcess(0);
}
