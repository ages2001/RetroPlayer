/* End-to-end UTF-8 SRT rendering through VLC/FreeType/HarfBuzz to memory.
 * The same solid video is decoded with subtitles disabled and enabled.
 * This checks visible raster output, not merely module load or text parsing. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <vlc/vlc.h>
#ifndef RP_SUB_FILE
#define RP_SUB_FILE "SUBS.SRT"
#define RP_REPORT_NAME "SUBTEST.TXT"
#define RP_LOG_NAME "SUBLOG.TXT"
#define RP_CAPTURE_NAME "SUBS.PPM"
#endif
#define APIS(X) X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) \
 X(libvlc_media_release) X(libvlc_media_add_option) X(libvlc_media_player_new_from_media) \
 X(libvlc_media_player_release) X(libvlc_media_player_play) X(libvlc_media_player_stop) \
 X(libvlc_video_set_callbacks) X(libvlc_video_set_format_callbacks) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH],option[MAX_PATH+32];
static unsigned checks,failures;
static volatile unsigned frames,bright_max;
static volatile unsigned ass_decoder;
static unsigned char pixels[115200] __attribute__((aligned(32)));
static unsigned char capture[76800];
extern int __cdecl __mingw_vsnprintf(char *,size_t,const char *,va_list);
#define CHECK(e) do { ++checks; if(!(e)) ++failures; } while(0)
static void line(const char *s) {
 DWORD n=(DWORD)lstrlenA(s),w;
 if(!WriteFile(report,s,n,&w,NULL) || w!=n) ExitProcess(8);
 FlushFileBuffers(report);
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list ap) {
 char text[1024];DWORD w;(void)opaque;(void)level;(void)ctx;
 __mingw_vsnprintf(text,sizeof(text)-1,fmt,ap);text[sizeof(text)-1]=0;fmt=text;
 if(strstr(text,"using spu decoder module \"libass\"")) ass_decoder=1;
 WriteFile(log_file,fmt,(DWORD)lstrlenA(fmt),&w,NULL);WriteFile(log_file,"\r\n",2,&w,NULL);
}
static unsigned setup(void **opaque,char *chroma,unsigned *width,unsigned *height,
                      unsigned *pitches,unsigned *lines) {
 (void)opaque;*width=320;*height=240;
 chroma[0]='I';chroma[1]='4';chroma[2]='2';chroma[3]='0';
 pitches[0]=320;pitches[1]=pitches[2]=160;
 lines[0]=240;lines[1]=lines[2]=120;return 1;
}
static void *lock(void *opaque,void **planes) {
 (void)opaque;planes[0]=pixels;planes[1]=pixels+76800;planes[2]=pixels+96000;planes[3]=NULL;return NULL;
}
static void unlock(void *opaque,void *picture,void *const *planes) {
 unsigned white=0;(void)opaque;(void)picture;(void)planes;
 for(unsigned i=0;i<76800;++i) if(pixels[i]>180) ++white;
 if(white>bright_max) {
  for(unsigned i=0;i<76800;++i) capture[i]=pixels[i];
  bright_max=white;
 }
 ++frames;
}
static void save(void) {
 DWORD written;unsigned char row[960];
 lstrcpyA(path,folder);lstrcatA(path,RP_CAPTURE_NAME);
 HANDLE file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 CHECK(file!=INVALID_HANDLE_VALUE);if(file==INVALID_HANDLE_VALUE) return;
 const char header[]="P6\n320 240\n255\n";
 CHECK(WriteFile(file,header,sizeof(header)-1,&written,NULL) && written==sizeof(header)-1);
 for(unsigned y=0;y<240;++y) {
  for(unsigned x=0;x<320;++x) row[3*x]=row[3*x+1]=row[3*x+2]=capture[320*y+x];
  CHECK(WriteFile(file,row,sizeof(row),&written,NULL) && written==sizeof(row));
 }
 CloseHandle(file);
}
int main(void) {
 DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);char *last=folder,text[160];
 if(!n || n>=MAX_PATH-24) ExitProcess(2);
 for(char *p=folder;*p;++p) if(*p=='\\') last=p+1;
 *last=0;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
 lstrcpyA(path,folder);lstrcatA(path,RP_REPORT_NAME);
 report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 if(report==INVALID_HANDLE_VALUE) ExitProcess(3);
 line("Scope: " RP_SUB_FILE " rasterization to I420; physical output untested.\r\n");
 lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
 if(!dll) { line("LOAD=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(4); }
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
 APIS(BIND)
 if(failures) { line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n");ExitProcess(5); }
 lstrcpyA(path,folder);lstrcatA(path,RP_LOG_NAME);
 log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
 for(unsigned pass=0;pass<2;++pass) {
  const char *args[]={"--ignore-config",
#if RP_USE_CACHE
   "--plugins-cache","--no-plugins-scan",
#else
   "--no-plugins-cache",
#endif
   "--no-media-library","--no-audio","--no-sub-autodetect-file",
   "--no-video-title-show","--avcodec-threads=2","--no-drop-late-frames","--no-skip-frames",
   "--file-caching=2000","--freetype-font=DejaVu Sans","--freetype-rel-fontsize=6",
#ifdef RP_ASS_TEST
   "--codec=libass,any",
#endif
   pass ? "--spu" : "--no-spu"};
  libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
  CHECK(instance!=NULL);if(!instance) continue;
  rp_libvlc_log_set(instance,logger,NULL);
  lstrcpyA(path,folder);lstrcatA(path,"SUBS.MP4");
  libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
  CHECK(media!=NULL);if(!media) { rp_libvlc_release(instance);continue; }
  if(pass) {
   lstrcpyA(option,":sub-file=");lstrcatA(option,folder);lstrcatA(option,RP_SUB_FILE);
   rp_libvlc_media_add_option(media,option);
  }
  libvlc_media_player_t *player=rp_libvlc_media_player_new_from_media(media);
  rp_libvlc_media_release(media);CHECK(player!=NULL);
  if(!player) { rp_libvlc_release(instance);continue; }
  frames=bright_max=0;
  rp_libvlc_video_set_callbacks(player,lock,unlock,NULL,NULL);
  rp_libvlc_video_set_format_callbacks(player,setup,NULL);
  CHECK(rp_libvlc_media_player_play(player)==0);
  DWORD start=GetTickCount();
  while(GetTickCount()-start<25000 && (frames<15 || (pass && bright_max<500))) Sleep(20);
  rp_libvlc_media_player_stop(player);
  CHECK(frames>=15);CHECK(pass ? bright_max>=500 : bright_max==0);
#ifdef RP_ASS_TEST
  if(pass)CHECK(ass_decoder==1);
#endif
  wsprintfA(text,"SUBTITLES=%s FRAMES=%u BRIGHT_PIXELS=%u\r\n",pass?"ON":"OFF",frames,bright_max);line(text);
  if(pass && bright_max>=500) save();
  rp_libvlc_media_player_release(player);rp_libvlc_release(instance);
 }
 CloseHandle(log_file);FreeLibrary(dll);
 wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
 line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
