/* Real demux/decode tests with independent frame hashes and audio reference. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#ifndef RP_MEDIA_HEADER
#define RP_MEDIA_HEADER "MEDDATA.H"
#endif
#include RP_MEDIA_HEADER
#ifndef RP_REPORT_NAME
#define RP_REPORT_NAME "MEDTEST.TXT"
#endif
#ifndef RP_LOG_NAME
#define RP_LOG_NAME "MEDLOG.TXT"
#endif
#ifndef RP_CODEC_THREADS
#define RP_CODEC_THREADS 1
#endif
#define STRING_VALUE_(x) #x
#define STRING_VALUE(x) STRING_VALUE_(x)
#if defined(RP_VIDEO_FILES)
#define VIDEO_FILES RP_VIDEO_FILES
#elif RP_WITH_AV1
#define VIDEO_FILES 5
#else
#define VIDEO_FILES 4
#endif
#define APIS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) \
 X(libvlc_media_player_play) X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
 X(libvlc_video_set_callbacks) X(libvlc_video_set_format_callbacks) \
 X(libvlc_audio_set_callbacks) X(libvlc_audio_set_format) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static char folder[MAX_PATH],path[MAX_PATH];
static unsigned checks,failures;
static volatile unsigned frame_count,frame_errors,matched,audio_count;
static volatile unsigned long long audio_sum;
static unsigned test_index;

static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#ifdef RP_RGB_TEST
static unsigned char pixels[9216] __attribute__((aligned(32)));
#else
static unsigned char pixels[4608] __attribute__((aligned(32)));
#endif
#if RP_PIXEL_CHECK
static unsigned char reference_pixels[12][4608];
static BOOL frame_matches(unsigned index,uint32_t hash)
{
    if(hash==frame_hashes[test_index][index])return TRUE;
    for(unsigned i=0;i<4608;++i) {
        int delta=(int)pixels[i]-reference_pixels[index][i];
        if(delta < -1 || delta > 1)return FALSE;
    }
    return TRUE;
}
#endif
#define CHECK(e) do { ++checks; if (!(e)) ++failures; } while (0)
static void line(const char *s)
{
    DWORD n=(DWORD)lstrlenA(s),w;
    if (!WriteFile(report,s,n,&w,NULL) || n!=w) ExitProcess(8);
    FlushFileBuffers(report);
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list ap)
{
    DWORD w;
    char text[1024];
    (void)opaque; (void)level; (void)ctx;
    if(format_log) { format_log(text,sizeof(text)-1,fmt,ap); text[sizeof(text)-1]=0; fmt=text; }
    WriteFile(log_file,fmt,(DWORD)lstrlenA(fmt),&w,NULL); WriteFile(log_file,"\r\n",2,&w,NULL);
}
static unsigned setup(void **opaque,char *chroma,unsigned *width,unsigned *height,
                      unsigned *pitches,unsigned *lines)
{
    (void)opaque;
    /* The decoder can expose padded coded dimensions. Negotiate our known
     * visible output size, as required by the LibVLC format callback ABI. */
    *width=64; *height=48;
#ifdef RP_RGB_TEST
    chroma[0]='R'; chroma[1]='V'; chroma[2]='2'; chroma[3]='4';
    pitches[0]=192; lines[0]=48;
    pitches[1]=pitches[2]=lines[1]=lines[2]=0;
#else
    chroma[0]='I'; chroma[1]='4'; chroma[2]='2'; chroma[3]='0';
    pitches[0]=64; pitches[1]=pitches[2]=32;
    lines[0]=48; lines[1]=lines[2]=24;
#endif
    return 1;
}
static void *video_lock(void *opaque,void **planes)
{
    (void)opaque;
    planes[0]=pixels;
#ifdef RP_RGB_TEST
    planes[1]=planes[2]=planes[3]=NULL;
#else
    planes[1]=pixels+3072; planes[2]=pixels+3840; planes[3]=NULL;
#endif
    return NULL;
}
static void video_unlock(void *opaque,void *picture,void *const *planes)
{
    unsigned i;
    uint32_t hash=2166136261UL;
    (void)opaque; (void)picture; (void)planes;
#ifdef RP_RGB_TEST
    if(!frame_count) {
        char capture_path[MAX_PATH];DWORD written;
        lstrcpyA(capture_path,folder);
        lstrcatA(capture_path,test_index ? "ZMBV.RGB" : "PNG.RGB");
        HANDLE file=CreateFileA(capture_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
        if(file!=INVALID_HANDLE_VALUE) {
            WriteFile(file,pixels,sizeof(pixels),&written,NULL);CloseHandle(file);
        }
    }
#endif
    for (i=0;i<sizeof(pixels);++i) hash=(hash^pixels[i])*16777619UL;
#if RP_PIXEL_CHECK
    if (matched<12 && frame_matches(matched,hash)) ++matched;
    else if (!matched || !frame_matches(matched-1,hash)) {
#else
    if (matched<12 && hash==frame_hashes[test_index][matched]) ++matched;
    else if (!matched || hash!=frame_hashes[test_index][matched-1]) {
#endif
#if RP_VIDEO_DUMP
        if (!frame_errors) {
            char capture_path[MAX_PATH]; DWORD written;
            lstrcpyA(capture_path,folder);
            wsprintfA(capture_path+lstrlenA(capture_path),"VF%02u.YUV",test_index);
            HANDLE file=CreateFileA(capture_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
            if(file!=INVALID_HANDLE_VALUE) {
                WriteFile(file,pixels,sizeof(pixels),&written,NULL);CloseHandle(file);
            }
        }
#endif
        ++frame_errors;
    }
    ++frame_count;
}
static void audio(void *opaque,const void *data,unsigned count,int64_t pts)
{
    const short *samples=data;
    unsigned i;
    unsigned long long sum=audio_sum;
    (void)opaque; (void)pts;
    for (i=0;i<count;++i) sum+=samples[i]<0 ? -(int)samples[i] : samples[i];
    audio_sum=sum; audio_count+=count;
}
void WINAPI RPEntry(void)
{
    HMODULE dll;
    libvlc_instance_t *instance;
    libvlc_media_t *media;
    libvlc_media_player_t *player;
    libvlc_state_t state;
    const char *args[]={"--ignore-config",
#if RP_USE_CACHE
        "--plugins-cache","--no-plugins-scan",
#else
        "--no-plugins-cache",
#endif
        "--no-media-library",
#if !RP_AUTO_DEMUX
        "--demux=avformat",
#endif
        "--no-drop-late-frames","--no-skip-frames",
        "--avcodec-threads=" STRING_VALUE(RP_CODEC_THREADS)
#if RP_WITH_AV1
        ,"--dav1d-thread-frames=" STRING_VALUE(RP_CODEC_THREADS)
#endif
    };
    char *p,*last,text[256];
    DWORD length,before;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    length=GetModuleFileNameA(NULL,folder,MAX_PATH);
    if (!length || length>=MAX_PATH-20) ExitProcess(2);
    last=folder; for(p=folder;*p;++p) if(*p=='\\') last=p+1; *last=0;
    lstrcpyA(path,folder); lstrcatA(path,RP_REPORT_NAME);
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(report==INVALID_HANDLE_VALUE) ExitProcess(3);
    line("Scope: software decode to memory; physical outputs and acceleration untested.\r\nSTEP=LOAD\r\n");
    line("CODEC_THREADS=" STRING_VALUE(RP_CODEC_THREADS) "\r\n");
#if RP_PIXEL_CHECK
    line("FRAME_CHECK=All YUV samples within one 8-bit unit of the host reference\r\n");
#endif
    lstrcpyA(path,folder); lstrcatA(path,"LIBVLC.DLL"); dll=LoadLibraryA(path);
    if(!dll) { line("LOAD=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(4); }
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n); CHECK(rp_##n!=NULL);
    APIS(BIND)
    format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
    if(failures) { line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(5); }
    instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
    if(!instance) { line("CREATE=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(6); }
    lstrcpyA(path,folder); lstrcatA(path,RP_LOG_NAME);
    log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(log_file==INVALID_HANDLE_VALUE) ExitProcess(7);
    rp_libvlc_log_set(instance,logger,NULL);
    for(test_index=0;test_index<=VIDEO_FILES;++test_index) {
        const char *name=test_index<VIDEO_FILES ? media_names[test_index] : "AAC.M4A";
        wsprintfA(text,"FILE=%s STEP=PLAY\r\n",name); line(text);
        frame_count=frame_errors=matched=audio_count=0; audio_sum=0;
#if RP_PIXEL_CHECK
        if(test_index<VIDEO_FILES) {
            DWORD read;
            lstrcpyA(path,folder);lstrcatA(path,reference_names[test_index]);
            HANDLE input=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
            if(input==INVALID_HANDLE_VALUE || !ReadFile(input,reference_pixels,sizeof(reference_pixels),&read,NULL) ||
               read!=sizeof(reference_pixels))ExitProcess(9);
            CloseHandle(input);
        }
#endif
        lstrcpyA(path,folder); lstrcatA(path,name);
        media=rp_libvlc_media_new_path(instance,path);
        if(!media) { ++failures; continue; }
        player=rp_libvlc_media_player_new_from_media(media); rp_libvlc_media_release(media);
        if(!player) { ++failures; continue; }
        rp_libvlc_video_set_callbacks(player,video_lock,video_unlock,NULL,NULL);
        rp_libvlc_video_set_format_callbacks(player,setup,NULL);
        rp_libvlc_audio_set_callbacks(player,audio,NULL,NULL,NULL,NULL,NULL);
        rp_libvlc_audio_set_format(player,"S16N",48000,1);
        CHECK(rp_libvlc_media_player_play(player)==0);
        before=GetTickCount(); state=libvlc_NothingSpecial;
        while(GetTickCount()-before<45000) {
            state=rp_libvlc_media_player_get_state(player);
            if(state==libvlc_Ended || state==libvlc_Error) break;
            Sleep(10);
        }
        /* Ended means input EOF; the display queue may still hold its final
         * scheduled picture. Do not cancel that picture with an early stop. */
        if(state==libvlc_Ended && test_index<VIDEO_FILES) {
            before=GetTickCount();
            while(matched<12 && GetTickCount()-before<1500) Sleep(10);
        }
        rp_libvlc_media_player_stop(player);
        CHECK(state==libvlc_Ended);
        if(test_index<VIDEO_FILES) { CHECK(matched==12); CHECK(frame_errors==0); }
        else {
            CHECK(audio_count==AAC_SAMPLES);
            CHECK(audio_sum>AAC_ABSUM*99/100 && audio_sum<AAC_ABSUM*101/100);
        }
        wsprintfA(text,"STATE=%u FRAMES=%u MATCHED=%u BAD=%u AUDIO=%u SUM=%lu\r\n",
            state,frame_count,matched,frame_errors,audio_count,(unsigned long)audio_sum); line(text);
        rp_libvlc_media_player_release(player);
    }
    rp_libvlc_release(instance); CloseHandle(log_file); FreeLibrary(dll);
    wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures?"FAIL":"PASS");
    line(text); CloseHandle(report); ExitProcess(failures?1:0);
}
