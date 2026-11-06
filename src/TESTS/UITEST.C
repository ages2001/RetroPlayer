/* Drive the real frontend, common file dialog and GDI playback controls. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dlgs.h>
static HANDLE report;
static unsigned checks,failures,first_failure;
static HWND frame,video;
static char folder[MAX_PATH],path[MAX_PATH],text[256];
#define CHECK(e) do {++checks;if(!(e)){++failures;if(!first_failure)first_failure=__LINE__;}} while(0)
static void line(const char *s)
{
 DWORD n=(DWORD)lstrlenA(s),w;
 if(!WriteFile(report,s,n,&w,NULL)||w!=n)ExitProcess(8);
 FlushFileBuffers(report);
}
static BOOL status_is(const char *expected)
{
 for(HWND child=GetWindow(frame,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
  /* GetWindowText does not retrieve another process's control text. */
  char label[256];SendMessageA(child,WM_GETTEXT,sizeof(label),(LPARAM)label);
  if(!lstrcmpA(label,expected))return TRUE;
 }
 return FALSE;
}
static BOOL wait_status(const char *expected)
{
 DWORD start=GetTickCount();
 while(GetTickCount()-start<45000) {
  if(status_is(expected))return TRUE;
  if(!IsWindow(frame))return FALSE;
  Sleep(50);
 }
 return FALSE;
}
static unsigned pixel(void)
{
 RECT rect;GetClientRect(video,&rect);HDC dc=GetDC(video);
 COLORREF color=GetPixel(dc,(rect.right-rect.left)/2,(rect.bottom-rect.top)/2);
 ReleaseDC(video,dc);
 if(color==CLR_INVALID)return 1000;
 return (GetRValue(color)+GetGValue(color)+GetBValue(color))/3;
}
static BOOL wait_bright(unsigned minimum)
{
 DWORD start=GetTickCount();
 while(GetTickCount()-start<15000) {
  unsigned value=pixel();
  if(value<256 && value>=minimum)return TRUE;
  Sleep(50);
 }
 return FALSE;
}
static BOOL wait_dark(unsigned maximum)
{
 DWORD start=GetTickCount();
 /* Playing can precede the first new GDI frame after asynchronous restart. */
 while(GetTickCount()-start<8000) {
  if(pixel()<=maximum)return TRUE;
  Sleep(50);
 }
 return FALSE;
}
static void phase(const char *name)
{
 wsprintfA(text,"PHASE=%s PIXEL=%u\r\n",name,pixel());line(text);
}
void WINAPI RPEntry(void)
{
 DWORD length=GetModuleFileNameA(NULL,folder,MAX_PATH),start;
 char source[MAX_PATH],command[MAX_PATH+4],*last=folder;
 WCHAR wide[MAX_PATH],wide_source[MAX_PATH];
 BOOL nt=(GetVersion()&0x80000000UL)==0;
 DWORD major=LOBYTE(LOWORD(GetVersion()));
 STARTUPINFOA startup={0};PROCESS_INFORMATION process;
 if(!length || length>=MAX_PATH-20)ExitProcess(2);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;
 lstrcpyA(path,folder);lstrcatA(path,"UITEST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 line("Scope: real frontend dialog, Unicode path, GDI pixels and playback menu.\r\n");
 lstrcpyA(source,folder);lstrcatA(source,"SEEK.MP4");
 lstrcpyA(path,folder);lstrcatA(path,"F\xe9.MP4");
 if(nt) {
  MultiByteToWideChar(CP_ACP,0,source,-1,wide_source,MAX_PATH);
  int n=MultiByteToWideChar(CP_ACP,0,folder,-1,wide,MAX_PATH);
  if(!n)ExitProcess(4);
  /* NT4 can store a name outside the ANSI code page on VFAT. Earlier NT
   * guests use a FAT-compatible accented short name. These are temporary
   * user media paths, never package filenames. */
  wide[n-1]='F';wide[n]=(major>=4?0x03a9:0x00e9);
  wide[n+1]='.';wide[n+2]='M';wide[n+3]='P';wide[n+4]='4';wide[n+5]=0;
  CHECK(CopyFileW(wide_source,wide,FALSE));
 } else CHECK(CopyFileA(source,path,FALSE));
 lstrcpyA(source,folder);lstrcatA(source,"RETRO.EXE");
 lstrcpyA(command,"\"");lstrcatA(command,source);lstrcatA(command,"\"");
 startup.cb=sizeof(startup);
 BOOL created=CreateProcessA(source,command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process);
 CHECK(created);
 if(!created){line("CREATE=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(5);}
 CloseHandle(process.hThread);
 start=GetTickCount();
 while(GetTickCount()-start<30000) {
  frame=FindWindowA("RetroPlayer",NULL);DWORD owner=0;
  if(frame)GetWindowThreadProcessId(frame,&owner);
  if(owner==process.dwProcessId)break;
  frame=NULL;Sleep(50);
 }
 CHECK(frame!=NULL);
 if(!frame)goto cleanup;
 CHECK(wait_status("LibVLC loaded; select File / Open."));
 for(HWND child=GetWindow(frame,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
  if((GetWindowLongA(child,GWL_STYLE)&0x1f)==SS_BLACKRECT)video=child;
 CHECK(video!=NULL);if(!video)goto cleanup;
 PostMessageA(frame,WM_COMMAND,101,0); /* File / Open */
 HWND dialog=NULL,edit=NULL;start=GetTickCount();
 while(GetTickCount()-start<15000) {
  dialog=GetLastActivePopup(frame);
  if(dialog!=frame && IsWindowVisible(dialog)) {
   /* Explorer-style dialogs keep edt1 hidden and use the filename combo. */
   edit=GetDlgItem(dialog,cmb13);
   if(!edit || !IsWindowVisible(edit))edit=GetDlgItem(dialog,edt1);
  }
  if(edit)break;
  Sleep(50);
 }
 CHECK(edit!=NULL);if(!edit)goto cleanup;
 wsprintfA(text,"FILE_CONTROL=%d VISIBLE=%u UNICODE=%u\r\n",GetDlgCtrlID(edit),
            (unsigned)IsWindowVisible(edit),(unsigned)IsWindowUnicode(edit));line(text);
 /* Old shell dialogs can expose the edit before folder initialization
  * finishes, then clear its initial text. Let the initial view settle and
  * verify the actual edit contents before accepting the selection. */
 Sleep(1500);
 if(nt)CHECK(SendMessageW(edit,WM_SETTEXT,0,(LPARAM)wide));
 else CHECK(SendMessageA(edit,WM_SETTEXT,0,(LPARAM)path));
 Sleep(500);
 if(nt) {
  WCHAR selected[MAX_PATH];SendMessageW(edit,WM_GETTEXT,MAX_PATH,(LPARAM)selected);
  CHECK(lstrcmpW(selected,wide)==0);
  wsprintfA(text,"EDIT_LENGTH=%u EXPECTED_LENGTH=%u\r\n",(unsigned)lstrlenW(selected),(unsigned)lstrlenW(wide));line(text);
 } else {
  char selected[MAX_PATH];SendMessageA(edit,WM_GETTEXT,MAX_PATH,(LPARAM)selected);
  CHECK(lstrcmpA(selected,path)==0);
 }
 PostMessageA(dialog,WM_COMMAND,IDOK,(LPARAM)GetDlgItem(dialog,IDOK));
 BOOL playing=wait_status("Playing");CHECK(playing);if(!playing)goto cleanup;
 Sleep(1200);
 PostMessageA(frame,WM_COMMAND,103,0);CHECK(wait_status("Paused"));Sleep(600);
 unsigned initial=pixel();CHECK(initial<80);phase("PAUSE");
 PostMessageA(frame,WM_COMMAND,108,0); /* Forward ten seconds while paused */
 CHECK(wait_bright(initial+30));CHECK(status_is("Paused"));phase("FORWARD");
 /* Reallocate the output surface while paused and require the same decoded
  * image after both size changes. A black cleared window cannot pass. */
 RECT original={0};BOOL saved=GetWindowRect(frame,&original);CHECK(saved);
 if(saved) {
  CHECK(MoveWindow(frame,10,10,380,300,TRUE));
  Sleep(300);CHECK(wait_bright(initial+30));CHECK(status_is("Paused"));phase("RESIZED");
  CHECK(MoveWindow(frame,original.left,original.top,original.right-original.left,
                   original.bottom-original.top,TRUE));
  Sleep(300);CHECK(wait_bright(initial+30));CHECK(status_is("Paused"));phase("RESTORED");
 }
 PostMessageA(frame,WM_COMMAND,107,0);Sleep(2000); /* Back ten seconds */
 CHECK(pixel()<initial+20);CHECK(status_is("Paused"));phase("BACKWARD");
 PostMessageA(frame,WM_COMMAND,109,0);Sleep(1500); /* Beginning */
 CHECK(pixel()<20);CHECK(status_is("Paused"));phase("BEGINNING");
 PostMessageA(frame,WM_COMMAND,103,0);CHECK(wait_status("Playing"));
 for(unsigned i=0;i<3;++i){PostMessageA(frame,WM_COMMAND,108,0);Sleep(1800);}
 CHECK(wait_status("Ended"));phase("ENDED");
 PostMessageA(frame,WM_COMMAND,102,0);CHECK(wait_status("Playing"));
 start=GetTickCount();CHECK(wait_dark(39));
 wsprintfA(text,"REPLAY_WAIT_MS=%lu\r\n",GetTickCount()-start);line(text);phase("REPLAY");
 /* A cleared black window is insufficient evidence of a new video frame.
  * Seek again after restart and require the expected bright/dark changes. */
 PostMessageA(frame,WM_COMMAND,103,0);CHECK(wait_status("Paused"));
 PostMessageA(frame,WM_COMMAND,108,0);CHECK(wait_bright(60));phase("REPLAY_FORWARD");
 PostMessageA(frame,WM_COMMAND,107,0);CHECK(wait_dark(39));phase("REPLAY_BACKWARD");
 PostMessageA(frame,WM_COMMAND,103,0);CHECK(wait_status("Playing"));
 /* Cancelling a subsequent file dialog must preserve the current media. */
 PostMessageA(frame,WM_COMMAND,101,0);Sleep(1000);
 dialog=GetLastActivePopup(frame);
 CHECK(dialog!=frame);
 if(dialog!=frame)PostMessageA(dialog,WM_COMMAND,IDCANCEL,(LPARAM)GetDlgItem(dialog,IDCANCEL));
 Sleep(300);PostMessageA(frame,WM_COMMAND,104,0);CHECK(wait_status("Stopped"));
 PostMessageA(frame,WM_COMMAND,102,0);CHECK(wait_status("Playing"));phase("CANCEL_REPLAY");
cleanup:
 if(frame) {
  HWND popup=GetLastActivePopup(frame);
  if(popup!=frame)PostMessageA(popup,WM_CLOSE,0,0);
  PostMessageA(frame,WM_CLOSE,0,0);
 }
 DWORD wait=WaitForSingleObject(process.hProcess,30000);CHECK(wait==WAIT_OBJECT_0);
 if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,ERROR_TIMEOUT);WaitForSingleObject(process.hProcess,5000);}
 DWORD code=1;CHECK(GetExitCodeProcess(process.hProcess,&code)&&code==0);CloseHandle(process.hProcess);
 if(nt)CHECK(DeleteFileW(wide));else CHECK(DeleteFileA(path));
 wsprintfA(text,"CHECKS=%u FAILURES=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",
            checks,failures,first_failure,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
