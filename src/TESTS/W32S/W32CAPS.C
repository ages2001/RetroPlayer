/* Win32s compatibility facts needed by the LibVLC engine (diagnostic only). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE out;
static void line(const char *s){DWORD n;WriteFile(out,s,lstrlenA(s),&n,NULL);}
static DWORD WINAPI worker(LPVOID p){*(volatile LONG*)p=1;return 7;}
void WINAPI RPEntry(void){
 char t[300],m[MAX_PATH];DWORD id,code;volatile LONG flag=0;HANDLE th,ev;HMODULE h;
 out=CreateFileA("C:\\RETROPLA\\W32CAPS.TXT",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(out==INVALID_HANDLE_VALUE)ExitProcess(1);
 wsprintfA(t,"GetVersion=%08lx\r\n",GetVersion());line(t);
 SetLastError(0);th=CreateThread(NULL,0,worker,(LPVOID)&flag,0,&id);
 wsprintfA(t,"CreateThread=%s error=%lu\r\n",th?"HANDLE":"NULL",GetLastError());line(t);
 if(th){WaitForSingleObject(th,2000);GetExitCodeThread(th,&code);wsprintfA(t,"thread ran=%ld exit=%lu\r\n",flag,code);line(t);CloseHandle(th);}
 SetLastError(0);ev=CreateEventA(NULL,FALSE,FALSE,NULL);
 wsprintfA(t,"CreateEvent=%s error=%lu\r\n",ev?"HANDLE":"NULL",GetLastError());line(t);
 if(ev){SetLastError(0);code=WaitForSingleObject(ev,100);wsprintfA(t,"WaitForSingleObject(timeout)=%lu error=%lu\r\n",code,GetLastError());line(t);CloseHandle(ev);}
 SetLastError(0);id=TlsAlloc();wsprintfA(t,"TlsAlloc=%lu error=%lu\r\n",id,GetLastError());line(t);
 GetModuleFileNameA(NULL,m,MAX_PATH);wsprintfA(t,"module(exe)=%s\r\n",m);line(t);
 h=LoadLibraryA("C:\\RETROPLA\\RPKERN.DLL");
 if(h){m[0]=0;GetModuleFileNameA(h,m,MAX_PATH);wsprintfA(t,"module(RPKERN)=%s\r\n",m);line(t);FreeLibrary(h);}
 else{wsprintfA(t,"LoadLibrary(RPKERN) error=%lu\r\n",GetLastError());line(t);}
 CloseHandle(out);ExitProcess(0);
}
