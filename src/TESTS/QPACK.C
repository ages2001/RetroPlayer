/* Exercise native Opus packets against a bounded legacy waveOut driver.
 * Closed device capture must independently verify the three tone sequences. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#define APIS(X) X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) X(libvlc_media_player_play) \
 X(libvlc_media_player_stop) X(libvlc_media_player_get_state) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH],log_buffer[262144];
static DWORD log_used,log_dropped;
static CRITICAL_SECTION log_lock;
static unsigned checks,failures,first;
static volatile LONG waveout,errors,native;
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(x) do{++checks;if(!(x)){++failures;if(!first)first=__LINE__;}}while(0)
static void line(const char *s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,NULL);FlushFileBuffers(report);}
static BOOL contains(const char *s,const char *p){for(;*s;++s){const char *a=s,*b=p;while(*b&&*a==*b){++a;++b;}if(!*b)return TRUE;}return FALSE;}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
 char text[1024];DWORD n;(void)opaque;(void)ctx;
 if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
 if(contains(fmt,"using audio output module \"waveout\""))waveout=1;
 if(level>=LIBVLC_ERROR&&(contains(fmt,"waveOut")||contains(fmt,"audio output failed")))++errors;
 n=lstrlenA(fmt);EnterCriticalSection(&log_lock);
 if(contains(fmt,"using demux module \"ogg\""))native|=1;
 if(contains(fmt,"using audio decoder module \"opus\""))native|=2;
 if(n+2<=sizeof(log_buffer)-log_used){for(DWORD i=0;i<n;++i)log_buffer[log_used++]=fmt[i];log_buffer[log_used++]='\r';log_buffer[log_used++]='\n';}else ++log_dropped;
 LeaveCriticalSection(&log_lock);
}
static DWORD WINAPI watchdog(void *unused){(void)unused;Sleep(120000);line("WATCHDOG=TIMEOUT\r\nRESULT=FAIL\r\n");ExitProcess(9);return 0;}
void WINAPI RPEntry(void)
{
 char text[160],*last=folder;DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH),written;
 if(!n||n>=MAX_PATH-20)ExitProcess(2);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 lstrcpyA(path,folder);lstrcatA(path,"QPACK.TXT");report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 line("Scope: 2.5/20/60 ms Opus packets through waveOut; independent captured tones required.\r\n");
 line("STEP=LOAD_ENGINE\r\n");
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);if(!dll)ExitProcess(5);
 line("STEP=ENGINE_LOADED\r\n");
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
 APIS(BIND)
 if(failures)ExitProcess(5);
 DWORD thread_id;
 HANDLE thread=CreateThread(NULL,0,watchdog,NULL,0,&thread_id);if(!thread)ExitProcess(4);CloseHandle(thread);

 lstrcpyA(path,folder);lstrcatA(path,"QPLOG.TXT");log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(log_file==INVALID_HANDLE_VALUE)ExitProcess(6);
 format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");InitializeCriticalSection(&log_lock);
 const char *files[]={"PACK2.OPU","PACK20.OPU","PACK60.OPU"};
 for(unsigned i=0;i<3;++i){
  waveout=errors=0;EnterCriticalSection(&log_lock);native=0;LeaveCriticalSection(&log_lock);
  lstrcpyA(path,folder);lstrcatA(path,files[i]);
  const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan","--no-media-library","--no-video","--no-spu","--aout=waveout","--no-waveout-float32","--file-caching=1000"};
  libvlc_instance_t *vlc=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);if(!vlc)ExitProcess(7);rp_libvlc_log_set(vlc,logger,NULL);
  libvlc_media_t *media=rp_libvlc_media_new_path(vlc,path);libvlc_media_player_t *player=media?rp_libvlc_media_player_new_from_media(media):NULL;if(media)rp_libvlc_media_release(media);if(!player)ExitProcess(7);
  DWORD before=GetTickCount();CHECK(rp_libvlc_media_player_play(player)==0);libvlc_state_t state;
  do{state=rp_libvlc_media_player_get_state(player);if(state==libvlc_Ended||state==libvlc_Error)break;Sleep(10);}while(GetTickCount()-before<30000);
  CHECK(state==libvlc_Ended);CHECK(waveout);CHECK(!errors);CHECK(native==3);Sleep(250);
  rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);rp_libvlc_release(vlc);
  wsprintfA(text,"FILE=%s STATE=%u ELAPSED_MS=%lu WAVEOUT=%ld ERRORS=%ld NATIVE=%ld\r\n",files[i],state,GetTickCount()-before,waveout,errors,native);line(text);Sleep(750);
 }
 CHECK(!log_dropped);CHECK(WriteFile(log_file,log_buffer,log_used,&written,NULL)&&written==log_used);CloseHandle(log_file);DeleteCriticalSection(&log_lock);FreeLibrary(dll);
 wsprintfA(text,"CHECKS=%u FAILURES=%u FIRST=%u\r\nRESULT=%s\r\n",checks,failures,first,failures?"FAIL":"PASS");line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
