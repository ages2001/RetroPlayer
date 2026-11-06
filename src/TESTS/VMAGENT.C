/* Private local-serial test harness. No network listener or arbitrary command
 * shell. Runs known tests and reads reports from its own application folder. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE port,child;
static char folder[MAX_PATH],path[MAX_PATH],command[MAX_PATH+4];
static BOOL send_bytes(const void *data,DWORD size) {
 const char *p=data;DWORD n;
 while(size) { if(!WriteFile(port,p,size>1024?1024:size,&n,NULL)||!n)return FALSE;p+=n;size-=n; }
 return TRUE;
}
static void reply(const char *s) { send_bytes(s,(DWORD)lstrlenA(s)); }
static BOOL filename(const char *s) {
 unsigned base=0,ext=0;BOOL dot=FALSE;
 for(;*s;++s) {
  if(*s=='.'&&!dot&&base){dot=TRUE;continue;}
  if(!((*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'))return FALSE;
  if(dot){if(++ext>3)return FALSE;}else if(++base>8)return FALSE;
 }
 return base&&dot&&ext;
}
static void get(const char *name) {
 DWORD size,n,hash=2166136261UL;char header[96];
 if(!filename(name)){reply("ERROR NAME\n");return;}
 lstrcpyA(path,folder);lstrcatA(path,name);
 HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(file==INVALID_HANDLE_VALUE){reply("ERROR OPEN\n");return;}
 size=GetFileSize(file,NULL);
 if(size>8*1024*1024){CloseHandle(file);reply("ERROR SIZE\n");return;}
 unsigned char *data=HeapAlloc(GetProcessHeap(),0,size?size:1);
 if(!data){CloseHandle(file);reply("ERROR MEMORY\n");return;}
 BOOL ok=ReadFile(file,data,size,&n,NULL)&&n==size;CloseHandle(file);
 if(!ok){HeapFree(GetProcessHeap(),0,data);reply("ERROR READ\n");return;}
 for(DWORD i=0;i<size;++i)hash=(hash^data[i])*16777619UL;
 wsprintfA(header,"DATA %lu %08lx\n",size,hash);reply(header);
 send_bytes(data,size);reply("\nEND\n");HeapFree(GetProcessHeap(),0,data);
}
static BOOL busy(void) { return child&&WaitForSingleObject(child,0)==WAIT_TIMEOUT; }
static void run(const char *name) {
 static const char *allowed[]={"CACHE.EXE","TXTTRY.EXE","MEDTRY.EXE","SUBTEST.EXE",
  "CPUTEST.EXE","GUARD.EXE","PCMTEST.EXE","MEDTEST.EXE","ZTEST.EXE","DEPTST.EXE",
  "CORETEST.EXE","CRTEST.EXE","KTEST.EXE","SYNCTEST.EXE","CXXTRY.EXE","ATTEST.EXE",
  "TLTEST.EXE","NETTEST.EXE","RETRO.EXE","FILETEST.EXE","ASSVID.EXE","SEEKTEST.EXE","UITEST.EXE","AOUTTEST.EXE","RAWTEST.EXE","AFMTEST.EXE","VFMTEST.EXE","INSTEST.EXE","AVTEST.EXE","AVFTEST.EXE","AVFIX.EXE","AVPCM.EXE","QUTEST.EXE","RESAMP.EXE","RESBASE.EXE","RESOLD.EXE","PRITEST.EXE","PRIOLD.EXE","ATTEST.EXE","SPTEST.EXE","SPRES.EXE","SROLD.EXE","QPACK.EXE","CLKTST.EXE","POSTEST.EXE","AV2TEST.EXE","AVPERF.EXE","CWTEST.EXE","BLTTEST.EXE","GDISUB.EXE","RSDIV.EXE","CLKPROBE.EXE"};
 BOOL accepted=FALSE;
 for(unsigned i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i)if(!lstrcmpA(name,allowed[i]))accepted=TRUE;
 if(!accepted){reply("ERROR TEST\n");return;}
 if(busy()){reply("ERROR BUSY\n");return;}
 if(child){CloseHandle(child);child=NULL;}
 STARTUPINFOA startup={0};PROCESS_INFORMATION process;
 lstrcpyA(path,folder);lstrcatA(path,name);
 lstrcpyA(command,"\"");lstrcatA(command,path);lstrcatA(command,"\"");startup.cb=sizeof(startup);
 if(!CreateProcessA(path,command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process)) {
  char text[64];wsprintfA(text,"ERROR CREATE %lu\n",GetLastError());reply(text);return;
 }
 child=process.hProcess;CloseHandle(process.hThread);reply("STARTED\n");
}
/* Clean guest shutdown for the host harness. NT requires the shutdown
 * privilege; Windows 95 has no token API, so failures there are ignored. */
static void shutdown_guest(void) {
 typedef BOOL (WINAPI *open_token)(HANDLE,DWORD,PHANDLE);
 typedef BOOL (WINAPI *lookup_value)(LPCSTR,LPCSTR,PLUID);
 typedef BOOL (WINAPI *adjust_token)(HANDLE,BOOL,PTOKEN_PRIVILEGES,DWORD,PTOKEN_PRIVILEGES,PDWORD);
 HMODULE advapi=LoadLibraryA("ADVAPI32.DLL");
 if(advapi) {
  open_token open=(open_token)GetProcAddress(advapi,"OpenProcessToken");
  lookup_value lookup=(lookup_value)GetProcAddress(advapi,"LookupPrivilegeValueA");
  adjust_token adjust=(adjust_token)GetProcAddress(advapi,"AdjustTokenPrivileges");
  HANDLE token;TOKEN_PRIVILEGES privileges;
  if(open&&lookup&&adjust&&open(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token)) {
   privileges.PrivilegeCount=1;privileges.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
   if(lookup(NULL,"SeShutdownPrivilege",&privileges.Privileges[0].Luid))
    adjust(token,FALSE,&privileges,0,NULL,NULL);
   CloseHandle(token);
  }
 }
 if(ExitWindowsEx(EWX_SHUTDOWN,0))reply("SHUTDOWN\n");
 else {char text[48];wsprintfA(text,"ERROR SHUTDOWN %lu\n",GetLastError());reply(text);}
}
void WINAPI RPEntry(void) {
 DWORD size=GetModuleFileNameA(NULL,folder,MAX_PATH);char *last=folder;
 if(!size||size>=MAX_PATH-16)ExitProcess(2);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 port=CreateFileA("COM1",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
 DWORD error=port==INVALID_HANDLE_VALUE?GetLastError():0;
 DCB state={0};state.DCBlength=sizeof(state);
 COMMTIMEOUTS timeouts={MAXDWORD,0,50,0,5000};
 if(!error) {
  if(!GetCommState(port,&state))error=GetLastError();
  else {
   state.BaudRate=115200;state.ByteSize=8;state.Parity=NOPARITY;state.StopBits=ONESTOPBIT;
   state.fBinary=TRUE;state.fParity=FALSE;state.fOutxCtsFlow=FALSE;state.fOutxDsrFlow=FALSE;
   state.fDtrControl=DTR_CONTROL_ENABLE;state.fDsrSensitivity=FALSE;
   state.fOutX=FALSE;state.fInX=FALSE;state.fErrorChar=FALSE;state.fNull=FALSE;
   state.fRtsControl=RTS_CONTROL_ENABLE;state.fAbortOnError=FALSE;
   if(!SetCommState(port,&state)||!SetCommTimeouts(port,&timeouts))error=GetLastError();
  }
 }
 lstrcpyA(path,folder);lstrcatA(path,"VMAGENT.TXT");
 HANDLE log=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 char text[96];wsprintfA(text,"COM1=%s ERROR=%lu\r\n",error?"FAIL":"READY",error);
 if(log!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(log,text,(DWORD)lstrlenA(text),&n,NULL);CloseHandle(log);}
 if(error)ExitProcess(3);
 char line[64];unsigned length=0;BOOL overflow=FALSE;
 for(;;) {
  char c;DWORD n;
  if(!ReadFile(port,&c,1,&n,NULL)){ClearCommError(port,&error,NULL);Sleep(20);continue;}
  if(!n)continue;
  if(c=='\r')continue;
  if(c!='\n'){if(length<sizeof(line)-1)line[length++]=c;else overflow=TRUE;continue;}
  line[length]=0;
  if(overflow)reply("ERROR LINE\n");
  else if(!lstrcmpA(line,"PING"))reply("PONG\n");
  else if(!lstrcmpA(line,"STAT")) {
   if(busy())reply("BUSY\n");
   else if(child){DWORD code;GetExitCodeProcess(child,&code);wsprintfA(text,"EXIT %lu\n",code);reply(text);}
   else reply("IDLE\n");
  }
  else if(!lstrcmpA(line,"QUIT")){reply("BYE\n");break;}
  else if(!lstrcmpA(line,"OFF")){if(busy())reply("ERROR BUSY\n");else shutdown_guest();}
  else if(length>4&&line[0]=='G'&&line[1]=='E'&&line[2]=='T'&&line[3]==' ')get(line+4);
  else if(length>4&&line[0]=='R'&&line[1]=='U'&&line[2]=='N'&&line[3]==' ')run(line+4);
  else reply("ERROR COMMAND\n");
  length=0;overflow=FALSE;
 }
 if(child)CloseHandle(child);
 CloseHandle(port);ExitProcess(0);
}
