/* Cold cache generation and cache-only metadata regression on the guest OS. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vlc/vlc.h>
static char folder[MAX_PATH],path[MAX_PATH];
static HANDLE report;
static unsigned checks,failures;
#define CHECK(e) do { ++checks;if(!(e))++failures; } while(0)
static void line(const char *s) {
 DWORD written,n=(DWORD)lstrlenA(s);
 if(!WriteFile(report,s,n,&written,NULL) || n!=written) ExitProcess(8);
 FlushFileBuffers(report);
}
void WINAPI RPEntry(void) {
 DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);char *last=folder,text[160];
 if(!n || n>=MAX_PATH-32)ExitProcess(2);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;
 lstrcpyA(path,folder);lstrcatA(path,"CACHE.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
 if(!dll){line("LOAD=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(4);}
 __typeof__(&libvlc_new) create=(__typeof__(&libvlc_new))GetProcAddress(dll,"libvlc_new");
 __typeof__(&libvlc_release) release=(__typeof__(&libvlc_release))GetProcAddress(dll,"libvlc_release");
 __typeof__(&libvlc_video_filter_list_get) list=(__typeof__(&libvlc_video_filter_list_get))GetProcAddress(dll,"libvlc_video_filter_list_get");
 __typeof__(&libvlc_module_description_list_release) free_list=(__typeof__(&libvlc_module_description_list_release))GetProcAddress(dll,"libvlc_module_description_list_release");
 if(!create||!release||!list||!free_list){line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(5);}
 unsigned cold=0;FILETIME generated={0},changed={0};
 for(unsigned pass=0;pass<2;++pass) {
  const char *args[]={"--ignore-config","--no-media-library",
    pass?"--plugins-cache":"--reset-plugins-cache",
    pass?"--no-plugins-scan":"--plugins-scan"};
  DWORD before=GetTickCount();
  libvlc_instance_t *instance=create(4,args);CHECK(instance!=NULL);
  if(!instance)continue;
  libvlc_module_description_t *head=list(instance);unsigned count=0;
  for(libvlc_module_description_t *item=head;item;item=item->p_next)++count;
  CHECK(count>0);if(!pass)cold=count;else CHECK(count==cold);
  free_list(head);release(instance);
  wsprintfA(text,"MODE=%s FILTERS=%u ELAPSED_MS=%lu\r\n",pass?"CACHE_ONLY":"GENERATE",count,GetTickCount()-before);line(text);
  lstrcpyA(path,folder);lstrcatA(path,"PLUGINS\\PLUGINS.DAT");
  HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  CHECK(file!=INVALID_HANDLE_VALUE);
  if(file!=INVALID_HANDLE_VALUE) {
   DWORD size=GetFileSize(file,NULL);CHECK(size!=INVALID_FILE_SIZE && size>32);
   CHECK(GetFileTime(file,NULL,NULL,pass?&changed:&generated));CloseHandle(file);
  }
 }
 CHECK(generated.dwLowDateTime==changed.dwLowDateTime && generated.dwHighDateTime==changed.dwHighDateTime);
 FreeLibrary(dll);wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
