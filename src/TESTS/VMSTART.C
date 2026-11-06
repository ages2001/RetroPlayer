/* Private test-disk launcher placed in a StartUp folder. Starts VMAGENT from
 * the RetroPlayer installation folder; never part of the player package. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI RPEntry(void) {
 static const char *paths[]={"C:\\RETROPLA\\VMAGENT.EXE",
  "C:\\Program Files\\RetroPlayer\\VMAGENT.EXE"};
 for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
  char folder[MAX_PATH],command[MAX_PATH+2];
  lstrcpyA(folder,paths[i]);
  char *last=folder;for(char *p=folder;*p;++p)if(*p=='\\')last=p;
  *last=0;
  lstrcpyA(command,"\"");lstrcatA(command,paths[i]);lstrcatA(command,"\"");
  STARTUPINFOA startup={0};PROCESS_INFORMATION process;startup.cb=sizeof(startup);
  if(GetFileAttributesA(paths[i])!=0xFFFFFFFF&&
     CreateProcessA(paths[i],command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process)) {
   CloseHandle(process.hThread);CloseHandle(process.hProcess);ExitProcess(0);
  }
 }
 ExitProcess(1);
}
