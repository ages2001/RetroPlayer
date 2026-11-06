/* Real LibVLC waveOut playback. The independent host capture must also pass;
 * an Ended state alone does not establish sound at the emulated device. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <vlc/vlc.h>
#define APIS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) \
 X(libvlc_media_player_play) X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
 X(libvlc_audio_set_volume) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH];
static unsigned checks,failures,first_failure;
static volatile LONG used_waveout,audio_errors;
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(e) do {++checks;if(!(e)){++failures;if(!first_failure)first_failure=__LINE__;}} while(0)
static void line(const char *s)
{
 DWORD n=(DWORD)lstrlenA(s),w;
 if(!WriteFile(report,s,n,&w,NULL)||n!=w)ExitProcess(8);
 FlushFileBuffers(report);
}
static BOOL contains(const char *s,const char *find)
{
 for(;*s;++s){const char *a=s,*b=find;while(*b && *a==*b){++a;++b;}if(!*b)return TRUE;}
 return FALSE;
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
 char text[1024];DWORD w;(void)opaque;(void)ctx;
 if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
 if(contains(fmt,"using audio output module \"waveout\""))used_waveout=1;
 if(level>=LIBVLC_ERROR && (contains(fmt,"waveOut") || contains(fmt,"audio output failed")))++audio_errors;
 WriteFile(log_file,fmt,(DWORD)lstrlenA(fmt),&w,NULL);WriteFile(log_file,"\r\n",2,&w,NULL);
}
/* Permit VLC's normal initial clock correction to settle before the tones.
 * RAWTEST separately measures the driver without clock correction. */
#define RP_PREROLL 20
#include "SOUND.H"
void WINAPI RPEntry(void)
{
 char *last=folder,text[256];DWORD length=GetModuleFileNameA(NULL,folder,MAX_PATH);
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 if(!length || length>=MAX_PATH-20)ExitProcess(2);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;lstrcpyA(path,folder);lstrcatA(path,"AOUTTEST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 line("Scope: LibVLC waveOut driver path; independent captured-audio analysis is required.\r\n");
 unsigned devices=waveOutGetNumDevs();wsprintfA(text,"WAVE_DEVICES=%u\r\n",devices);line(text);
 if(!devices){line("REASON=No installed wave output device\r\nRESULT=BLOCKED\r\n");CloseHandle(report);ExitProcess(77);}
 for(unsigned i=0;i<devices;++i){WAVEOUTCAPSA caps;MMRESULT rc=waveOutGetDevCapsA(i,&caps,sizeof(caps));
  CHECK(rc==MMSYSERR_NOERROR);if(!rc){wsprintfA(text,"DEVICE=%u NAME=%s CHANNELS=%u\r\n",i,caps.szPname,caps.wChannels);line(text);}}
 CHECK(make_wave());if(failures){line("FIXTURE=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(4);}
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
 if(!dll){line("LOAD=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(4);}
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
 APIS(BIND)
 if(failures){line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(5);}
 const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan",
  "--no-video","--no-media-library","--aout=waveout","--no-waveout-float32","--file-caching=1000"};
 libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
 if(!instance){line("CREATE=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 lstrcpyA(path,folder);lstrcatA(path,"AOUTLOG.TXT");
 log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(log_file==INVALID_HANDLE_VALUE)ExitProcess(7);
 format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
 rp_libvlc_log_set(instance,logger,NULL);
 lstrcpyA(path,folder);lstrcatA(path,"SNDTEST.WAV");libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
 if(!media){line("MEDIA=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 libvlc_media_player_t *player=rp_libvlc_media_player_new_from_media(media);rp_libvlc_media_release(media);
 if(!player){line("PLAYER=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 CHECK(rp_libvlc_audio_set_volume(player,100)==0);CHECK(rp_libvlc_media_player_play(player)==0);
 DWORD before=GetTickCount();libvlc_state_t state;
 do{state=rp_libvlc_media_player_get_state(player);if(state==libvlc_Ended || state==libvlc_Error)break;Sleep(20);}
 while(GetTickCount()-before<65000);
 CHECK(state==libvlc_Ended);CHECK(used_waveout);CHECK(audio_errors==0);
 rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);rp_libvlc_release(instance);
 wsprintfA(text,"STATE=%u WAVEOUT=%ld AUDIO_ERRORS=%ld\r\n",state,used_waveout,audio_errors);line(text);
 CloseHandle(log_file);FreeLibrary(dll);
 wsprintfA(text,"CHECKS=%u FAILURES=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",checks,failures,first_failure,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
