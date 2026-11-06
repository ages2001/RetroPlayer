#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* Minimal Win32s diagnostic: write a report, then show a message box. */
static void line(HANDLE f,const char *s){DWORD n;WriteFile(f,s,lstrlenA(s),&n,NULL);}
void WINAPI RPEntry(void){
 char t[160];DWORD v=GetVersion();
 HANDLE f=CreateFileA("C:\\RETROPLA\\W32HELLO.TXT",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 wsprintfA(t,"VERSION=%08lx\r\n",v);
 if(f!=INVALID_HANDLE_VALUE){line(f,t);CloseHandle(f);}
 MessageBoxA(NULL,t,"W32HELLO",MB_OK);
 ExitProcess(0);
}
