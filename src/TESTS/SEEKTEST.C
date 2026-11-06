/* Exercise real pause/seek/end/replay using independently identified frames.
 * Frame arrival is observed at display, not decoder pre-roll. No physical
 * output, audio synchronization, or other demuxer is covered by this test. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#include "SEEKDATA.H"
#define APIS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) \
 X(libvlc_media_player_play) X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
 X(libvlc_media_player_set_pause) X(libvlc_media_player_set_time) \
 X(libvlc_media_player_is_seekable) X(libvlc_media_player_can_pause) \
 X(libvlc_media_player_get_length) X(libvlc_video_set_callbacks) \
 X(libvlc_video_set_format_callbacks) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH];
static unsigned checks,failures,first_failure;
static volatile LONG displayed,bad_frames,last_frame=-1;
static LONG pending_frame;
static unsigned char pixels[4608] __attribute__((aligned(32)));
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(e) do { ++checks;if(!(e)){++failures;if(!first_failure)first_failure=__LINE__;} } while(0)
static void line(const char *s)
{
 DWORD size=(DWORD)lstrlenA(s),written;
 if(!WriteFile(report,s,size,&written,NULL)||size!=written)ExitProcess(8);
 FlushFileBuffers(report);
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
 char text[1024];DWORD written;(void)opaque;(void)level;(void)ctx;
 if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
 WriteFile(log_file,fmt,(DWORD)lstrlenA(fmt),&written,NULL);
 WriteFile(log_file,"\r\n",2,&written,NULL);
}
static unsigned setup(void **opaque,char *chroma,unsigned *width,unsigned *height,
                      unsigned *pitches,unsigned *lines)
{
 (void)opaque;*width=64;*height=48;
 chroma[0]='I';chroma[1]='4';chroma[2]='2';chroma[3]='0';
 pitches[0]=64;pitches[1]=pitches[2]=32;lines[0]=48;lines[1]=lines[2]=24;
 return 1;
}
static void *lock(void *opaque,void **planes)
{
 (void)opaque;planes[0]=pixels;planes[1]=pixels+3072;planes[2]=pixels+3840;
 return NULL;
}
static void unlock(void *opaque,void *picture,void *const *planes)
{
 uint32_t hash=2166136261UL;(void)opaque;(void)picture;(void)planes;
 for(unsigned i=0;i<sizeof(pixels);++i)hash=(hash^pixels[i])*16777619UL;
 pending_frame=-1;
 for(unsigned i=0;i<60;++i)if(hash==reference[i]){pending_frame=(LONG)i;break;}
}
static void display(void *opaque,void *picture)
{
 (void)opaque;(void)picture;
 if(pending_frame<0)++bad_frames;
 last_frame=pending_frame;++displayed;
}
static BOOL await_frame(libvlc_media_player_t *player,LONG low,LONG high)
{
 DWORD start=GetTickCount();
 while(GetTickCount()-start<30000) {
  LONG frame=last_frame;
  if(frame>=low && frame<=high)return TRUE;
  libvlc_state_t state=rp_libvlc_media_player_get_state(player);
  if(state==libvlc_Error || state==libvlc_Ended)return FALSE;
  Sleep(20);
 }
 return FALSE;
}
static BOOL await_state(libvlc_media_player_t *player,libvlc_state_t desired)
{
 DWORD start=GetTickCount();
 while(GetTickCount()-start<30000) {
  libvlc_state_t state=rp_libvlc_media_player_get_state(player);
  if(state==desired)return TRUE;
  if(state==libvlc_Error)return FALSE;
  Sleep(20);
 }
 return FALSE;
}
static void phase(const char *name)
{
 char text[160];wsprintfA(text,"PHASE=%s FRAME=%ld DISPLAYS=%ld BAD=%ld\r\n",
                         name,last_frame,displayed,bad_frames);line(text);
}
void WINAPI RPEntry(void)
{
 char text[256],*last;DWORD size=GetModuleFileNameA(NULL,folder,MAX_PATH);
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 if(!size || size>=MAX_PATH-20)ExitProcess(2);
 last=folder;for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;*last=0;
 lstrcpyA(path,folder);lstrcatA(path,"SEEKTEST.TXT");
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
 line("Scope: H.264 MP4 pause/seek/replay to memory; physical output and A/V sync untested.\r\n");
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
 if(!dll){line("LOAD=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(4);}
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
 APIS(BIND)
 if(failures){line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(5);}
 format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
 const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan",
  "--no-media-library","--no-audio","--no-sub-autodetect-file","--no-spu",
  "--no-drop-late-frames","--no-skip-frames","--avcodec-threads=2","--file-caching=1000"};
 libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
 if(!instance){line("CREATE=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 lstrcpyA(path,folder);lstrcatA(path,"SEEKLOG.TXT");
 log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(log_file==INVALID_HANDLE_VALUE)ExitProcess(7);
 rp_libvlc_log_set(instance,logger,NULL);
 lstrcpyA(path,folder);lstrcatA(path,"SEEK.MP4");
 libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
 if(!media){line("MEDIA=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 libvlc_media_player_t *player=rp_libvlc_media_player_new_from_media(media);
 rp_libvlc_media_release(media);
 if(!player){line("PLAYER=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(6);}
 rp_libvlc_video_set_callbacks(player,lock,unlock,display,NULL);
 rp_libvlc_video_set_format_callbacks(player,setup,NULL);
 CHECK(rp_libvlc_media_player_play(player)==0);
 CHECK(await_frame(player,2,10));phase("START");
 CHECK(rp_libvlc_media_player_is_seekable(player));
 CHECK(rp_libvlc_media_player_can_pause(player));
 CHECK(rp_libvlc_media_player_get_length(player)==30000);
 rp_libvlc_media_player_set_pause(player,1);
 CHECK(await_state(player,libvlc_Paused));Sleep(1200);
 LONG frozen=last_frame;Sleep(1200);
 CHECK(last_frame==frozen);CHECK(rp_libvlc_media_player_get_state(player)==libvlc_Paused);
 phase("PAUSED");
 rp_libvlc_media_player_set_pause(player,0);
 CHECK(await_frame(player,frozen+1,frozen+8));phase("RESUMED");
 rp_libvlc_media_player_set_time(player,20000);
 CHECK(await_frame(player,40,46));phase("FORWARD");
 rp_libvlc_media_player_set_time(player,3000);
 CHECK(await_frame(player,6,12));phase("BACKWARD");
 rp_libvlc_media_player_set_time(player,28000);
 CHECK(await_frame(player,56,59));phase("NEAR_END");
 CHECK(await_state(player,libvlc_Ended));phase("ENDED");
 /* The frontend uses this same stop/play transition after Ended. */
 rp_libvlc_media_player_stop(player);CHECK(rp_libvlc_media_player_get_state(player)==libvlc_Stopped);
 last_frame=-1;CHECK(rp_libvlc_media_player_play(player)==0);
 CHECK(await_frame(player,0,8));phase("REPLAY");
 rp_libvlc_media_player_stop(player);CHECK(rp_libvlc_media_player_get_state(player)==libvlc_Stopped);
 CHECK(bad_frames==0);
 rp_libvlc_media_player_release(player);rp_libvlc_release(instance);
 CloseHandle(log_file);FreeLibrary(dll);
 wsprintfA(text,"CHECKS=%u FAILURES=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",
            checks,failures,first_failure,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
