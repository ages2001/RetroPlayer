/* Native Speex decode comparison, using independent host PCM references. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#include "SPDATA.H"
#define APIS(X) X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) X(libvlc_media_player_play) \
 X(libvlc_media_player_stop) X(libvlc_media_player_get_state) X(libvlc_audio_set_callbacks) \
 X(libvlc_audio_set_format) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
#define CAPACITY 96000
static short decoded[CAPACITY],reference[CAPACITY];
static volatile unsigned samples,overflow,channels,native_modules;
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH],log_buffer[262144];
static DWORD log_used,log_dropped;
static CRITICAL_SECTION log_lock;
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
static void line(const char *s){DWORD w,n=lstrlenA(s);if(!WriteFile(report,s,n,&w,NULL)||w!=n)ExitProcess(9);FlushFileBuffers(report);}
static BOOL contains(const char *s,const char *p){for(;*s;++s){const char *a=s,*b=p;while(*b&&*a==*b){++a;++b;}if(!*b)return TRUE;}return FALSE;}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
 char text[1024];DWORD n;(void)opaque;(void)level;(void)ctx;
 if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
 EnterCriticalSection(&log_lock);
 if(contains(fmt,"using demux module \"ogg\""))native_modules|=1;
 if(contains(fmt,"using audio decoder module \"speex\""))native_modules|=2;
 n=lstrlenA(fmt);
 if(n+2<=sizeof(log_buffer)-log_used){for(DWORD i=0;i<n;++i)log_buffer[log_used++]=fmt[i];log_buffer[log_used++]='\r';log_buffer[log_used++]='\n';}else ++log_dropped;
 LeaveCriticalSection(&log_lock);
}
static void play(void *opaque,const void *buffer,unsigned count,int64_t pts)
{
 (void)opaque;(void)pts;
 if(!channels||count>(CAPACITY-samples)/channels){overflow=1;return;}
 count*=channels;CopyMemory(decoded+samples,buffer,count*sizeof(short));samples+=count;
}
static void pause_audio(void *p,int64_t pts){(void)p;(void)pts;}
static void flush_audio(void *p,int64_t pts){(void)p;(void)pts;}
static void drain_audio(void *p){(void)p;}
void WINAPI RPEntry(void)
{
 char text[256],*last=folder;unsigned failures=0;DWORD length=GetModuleFileNameA(NULL,folder,MAX_PATH);
 if(!length||length>=MAX_PATH-20)ExitProcess(1);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 lstrcpyA(path,folder);lstrcatA(path,"SPTEST.TXT");report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("Scope: native Speex PCM at original rates/channels; full-stream comparison, no device output.\r\n");
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);if(!dll)ExitProcess(3);
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);if(!rp_##n)ExitProcess(4);
 APIS(BIND)
 const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan","--no-video","--no-media-library","--file-caching=1000","--avcodec-threads=2"};
 libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);if(!instance)ExitProcess(5);
 lstrcpyA(path,folder);lstrcatA(path,"SPLOG.TXT");log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(log_file==INVALID_HANDLE_VALUE)ExitProcess(6);
 format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");InitializeCriticalSection(&log_lock);rp_libvlc_log_set(instance,logger,NULL);
 for(unsigned test=0;test<sizeof(cases)/sizeof(cases[0]);++test){
  DWORD read=0,written;unsigned errors=0;
  lstrcpyA(path,folder);lstrcatA(path,cases[test].raw);HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if(f==INVALID_HANDLE_VALUE||!ReadFile(f,reference,sizeof(reference),&read,NULL)||read<2)ExitProcess(7);
  CloseHandle(f);
  unsigned expected=read/2;samples=overflow=0;channels=cases[test].channels;
  EnterCriticalSection(&log_lock);native_modules=0;LeaveCriticalSection(&log_lock);
  lstrcpyA(path,folder);lstrcatA(path,cases[test].file);
  libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);libvlc_media_player_t *player=media?rp_libvlc_media_player_new_from_media(media):NULL;
  if(media)rp_libvlc_media_release(media);
  if(!player)ExitProcess(8);
  rp_libvlc_audio_set_callbacks(player,play,pause_audio,pause_audio,flush_audio,drain_audio,NULL);rp_libvlc_audio_set_format(player,"S16N",cases[test].rate,channels);
  if(rp_libvlc_media_player_play(player))++errors;
  DWORD start=GetTickCount();libvlc_state_t state;
  do{state=rp_libvlc_media_player_get_state(player);if(state==libvlc_Ended||state==libvlc_Error)break;Sleep(20);}while(GetTickCount()-start<20000);
  rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);
  unsigned count=samples,compared=count<expected?count:expected,max_error=0;uint64_t squared=0,energy=0;
  if(state!=libvlc_Ended||overflow||count!=expected||!count||native_modules!=3)++errors;
  for(unsigned i=0;i<compared;++i){int delta=(int)decoded[i]-reference[i];unsigned absolute=delta<0?-delta:delta;if(absolute>max_error)max_error=absolute;squared+=(int64_t)delta*delta;energy+=(int64_t)reference[i]*reference[i];}
  if(!compared||energy<(uint64_t)compared*1000||squared*10000>energy)++errors;
  wsprintfA(text,"FILE=%s RATE=%u CHANNELS=%u SAMPLES=%u EXPECTED=%u NATIVE=%u\r\n",cases[test].file,cases[test].rate,channels,count,expected,native_modules);line(text);
  wsprintfA(text,"MAX_ERROR=%u MSE=%lu SIGNAL=%lu STATE=%u ERRORS=%u RESULT=%s\r\n",max_error,(unsigned long)(squared/(compared?compared:1)),(unsigned long)(energy/(compared?compared:1)),state,errors,errors?"FAIL":"PASS");line(text);
  if(errors&&count){lstrcpyA(path,folder);wsprintfA(path+lstrlenA(path),"SP%02u.OUT",test);HANDLE dump=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(dump!=INVALID_HANDLE_VALUE){WriteFile(dump,decoded,count*2,&written,NULL);CloseHandle(dump);}}
  if(errors)++failures;
 }
 rp_libvlc_release(instance);DWORD written;
 if(log_dropped||!WriteFile(log_file,log_buffer,log_used,&written,NULL)||written!=log_used)++failures;
 CloseHandle(log_file);DeleteCriticalSection(&log_lock);FreeLibrary(dll);
 wsprintfA(text,"CASES=%u FAILURES=%u\r\nRESULT=%s\r\n",(unsigned)(sizeof(cases)/sizeof(cases[0])),failures,failures?"FAIL":"PASS");line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
