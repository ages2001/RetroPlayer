/* Compare decoded audio with independent host references, allowing codec delay. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#include "AFDATA.H"
#define APIS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) \
 X(libvlc_media_player_play) X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
 X(libvlc_audio_set_callbacks) X(libvlc_audio_set_format) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
#define CAPACITY 192000
static short decoded[CAPACITY],reference[CAPACITY];
static volatile unsigned samples,overflow;
static HANDLE report,log_file;
static volatile unsigned native_modules;
static char folder[MAX_PATH],path[MAX_PATH];
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
static void line(const char *s)
{
 DWORD w,n=lstrlenA(s);if(!WriteFile(report,s,n,&w,NULL)||w!=n)ExitProcess(9);
 FlushFileBuffers(report);
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
 char text[1024];DWORD w;(void)opaque;(void)level;(void)ctx;
 if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
 const char *markers[]={"using demux module \"ogg\"","using audio decoder module \"opus\"",
                        "using audio decoder module \"vorbis\"","using audio decoder module \"flac\""};
 for(unsigned i=0;i<4;++i) {
  for(const char *p=fmt;*p;++p){const char *a=p,*b=markers[i];while(*b&&*a==*b){++a;++b;}
   if(!*b){native_modules|=1u<<i;break;}}
 }
 WriteFile(log_file,fmt,lstrlenA(fmt),&w,NULL);WriteFile(log_file,"\r\n",2,&w,NULL);
}
static void play(void *opaque,const void *buffer,unsigned count,int64_t pts)
{
 (void)opaque;(void)pts;
 if(count>CAPACITY-samples){overflow=1;return;}
 CopyMemory(decoded+samples,buffer,count*sizeof(short));samples+=count;
}
static void pause_audio(void *p,int64_t pts){(void)p;(void)pts;}
static void flush_audio(void *p,int64_t pts){(void)p;(void)pts;}
static void drain_audio(void *p){(void)p;}
void WINAPI RPEntry(void)
{
 char text[256],*last=folder;unsigned failures=0;
 GetModuleFileNameA(NULL,folder,MAX_PATH);
 for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
 *last=0;lstrcpyA(path,folder);lstrcatA(path,"AFMTEST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 line("Scope: decoded PCM comparison with independent host references; no device output.\r\n");
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
 if(!dll)ExitProcess(3);
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);if(!rp_##n)ExitProcess(4);
 APIS(BIND)
 /* Decode-content coverage must not depend on a 100 ms startup budget on
  * the 32 MiB NT3.1 VM. Device timing is tested separately by AOUTTEST. */
 const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan","--no-video",
                    "--no-media-library","--file-caching=1000","--avcodec-threads=2"};
 libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
 if(!instance)ExitProcess(5);
 lstrcpyA(path,folder);lstrcatA(path,"AFMLOG.TXT");
 log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(log_file==INVALID_HANDLE_VALUE)ExitProcess(6);
 format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
 rp_libvlc_log_set(instance,logger,NULL);
 for(unsigned test=0;test<sizeof(cases)/sizeof(cases[0]);++test){
  DWORD read=0;unsigned errors=0;int lag=0;uint64_t best=UINT64_MAX;
  lstrcpyA(path,folder);lstrcatA(path,cases[test].raw);
  HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if(f==INVALID_HANDLE_VALUE||!ReadFile(f,reference,sizeof(reference),&read,NULL)||read<2)ExitProcess(7);
  CloseHandle(f);unsigned expected=read/2;samples=overflow=native_modules=0;
  lstrcpyA(path,folder);lstrcatA(path,cases[test].file);
  libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
  libvlc_media_player_t *player=media?rp_libvlc_media_player_new_from_media(media):NULL;
  if(media)rp_libvlc_media_release(media);
  if(!player)ExitProcess(8);
  rp_libvlc_audio_set_callbacks(player,play,pause_audio,pause_audio,flush_audio,drain_audio,NULL);
  rp_libvlc_audio_set_format(player,"S16N",48000,1);
  if(rp_libvlc_media_player_play(player))++errors;
  DWORD start=GetTickCount();libvlc_state_t state;
  do{state=rp_libvlc_media_player_get_state(player);if(state==libvlc_Ended||state==libvlc_Error)break;Sleep(20);}
  while(GetTickCount()-start<20000);
  rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);
  unsigned count=samples;
  if(state!=libvlc_Ended||overflow||count+4096<expected||expected+4096<count||!count)++errors;
  uint64_t squared=0,energy=0;unsigned compared=0,max_error=0;
  if(count){
   /* Find encoder/container delay using a chirp with deterministic noise.
    * The following whole-stream comparison prevents a short match passing. */
   for(int shift=-4096;count>=20000 && expected>=7000 && shift<=4096;++shift){
    uint64_t difference=0;
    for(unsigned i=5000;i<7000;i+=8){int d=(int)decoded[i+shift]-reference[i];difference+=(int64_t)d*d;}
    if(difference<best){best=difference;lag=shift;}
   }
   unsigned begin=lag<0?(unsigned)-lag:0,end=expected;
   if((int64_t)end+lag>count)end=count-lag;
   for(unsigned i=begin;i<end;++i){
    int d=(int)decoded[i+lag]-reference[i];unsigned a=d<0?-d:d;
    if(a>max_error)max_error=a;
    squared+=(int64_t)d*d;
    energy+=(int64_t)reference[i]*reference[i];++compared;
   }
   /* At least 95% coverage, 40 dB signal/error ratio, and non-silent output. */
   if(compared*100<expected*95||energy<(uint64_t)compared*1000||squared*10000>energy)++errors;
  }
  unsigned required=0;
  if(!lstrcmpA(cases[test].file+lstrlenA(cases[test].file)-4,".OPU"))required=3;
  if(!lstrcmpA(cases[test].file,"OPUSMKV.MKA"))required=2;
  if(!lstrcmpA(cases[test].file+lstrlenA(cases[test].file)-4,".OGG"))required=5;
  if(!lstrcmpA(cases[test].file,"FLAC.FLA"))required=8;
  if((native_modules&required)!=required)++errors;
  if(required && (count!=expected || lag!=0))++errors;
  wsprintfA(text,"NATIVE=%u REQUIRED=%u\r\n",native_modules,required);line(text);
  wsprintfA(text,"FILE=%s SAMPLES=%u EXPECTED=%u LAG=%d COMPARED=%u MAX_ERROR=%u\r\n",
            cases[test].file,count,expected,lag,compared,max_error);line(text);
  wsprintfA(text,"MSE=%lu SIGNAL=%lu\r\n",(unsigned long)(squared/(compared?compared:1)),
            (unsigned long)(energy/(compared?compared:1)));line(text);
  if(errors && count){
   lstrcpyA(path,folder);wsprintfA(path+lstrlenA(path),"AF%02u.OUT",test);
   HANDLE dump=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);DWORD written;
   if(dump!=INVALID_HANDLE_VALUE){WriteFile(dump,decoded,count*2,&written,NULL);CloseHandle(dump);}
  }
  wsprintfA(text,"STATE=%u ERRORS=%u RESULT=%s\r\n",state,errors,errors?"FAIL":"PASS");line(text);
  if(errors)++failures;
 }
 rp_libvlc_release(instance);CloseHandle(log_file);FreeLibrary(dll);
 wsprintfA(text,"FORMATS=%u FAILURES=%u\r\nRESULT=%s\r\n",(unsigned)(sizeof(cases)/sizeof(cases[0])),failures,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
