/* Render owned audiovisual markers through real RPGDI and waveOut outputs.
 * The host must independently compare captured pixels with captured sound. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef RP_AV_PERF
#include <mmsystem.h>
#endif
#include <stdint.h>
#include <vlc/vlc.h>
#ifdef RP_AV_PCM
#define PCM_APIS(X) X(libvlc_audio_set_callbacks) X(libvlc_audio_set_format)
#else
#define PCM_APIS(X)
#endif
#define APIS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_player_new_from_media) X(libvlc_media_player_release) \
 X(libvlc_media_player_play) X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
 X(libvlc_media_player_set_hwnd) X(libvlc_audio_set_volume) X(libvlc_log_set) PCM_APIS(X)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report,log_file;
static HWND window;
static char folder[MAX_PATH],path[MAX_PATH];
static unsigned checks,failures,first_failure;
static volatile LONG waveout,amem,gdi,audio_errors;
static CRITICAL_SECTION log_lock;
static char log_buffer[262144];
static DWORD log_used,log_dropped;
#ifdef RP_AV_PCM
static int16_t decoded[480000];
static unsigned decoded_count,decoded_overflow;
static void pcm_play(void *opaque,const void *data,unsigned count,int64_t pts)
{
    const int16_t *input=data;(void)opaque;(void)pts;
    if(count>480000-decoded_count){decoded_overflow=1;return;}
    for(unsigned i=0;i<count;++i)decoded[decoded_count++]=input[i];
}
#endif
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(x) do{++checks;if(!(x)){++failures;if(!first_failure)first_failure=__LINE__;}}while(0)
static void line(const char *s)
{
    DWORD n=lstrlenA(s),written;
    if(!WriteFile(report,s,n,&written,NULL)||written!=n)ExitProcess(8);
    FlushFileBuffers(report);
}
static BOOL contains(const char *s,const char *part)
{
    for(;*s;++s){const char *a=s,*b=part;while(*b&&*a==*b){++a;++b;}if(!*b)return TRUE;}
    return FALSE;
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
    char text[1024];DWORD n;(void)opaque;(void)ctx;
    if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
    if(contains(fmt,"using audio output module \"waveout\""))waveout=1;
    if(contains(fmt,"using audio output module \"amem\""))amem=1;
    if(contains(fmt,"using vout display module \"RPGDI\""))gdi=1;
    if(level>=LIBVLC_ERROR&&(contains(fmt,"waveOut")||contains(fmt,"audio output failed")))++audio_errors;
    /* Disk writes from decoder callbacks perturb legacy real-time playback.
     * Preserve diagnostics in bounded memory and write after playback. */
    n=lstrlenA(fmt);EnterCriticalSection(&log_lock);
    if(n+2<=sizeof(log_buffer)-log_used){
        for(DWORD i=0;i<n;++i)log_buffer[log_used++]=fmt[i];
        log_buffer[log_used++]='\r';log_buffer[log_used++]='\n';
    }else ++log_dropped;
    LeaveCriticalSection(&log_lock);
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l)
{
    if(message==WM_CLOSE)return 0;
    return DefWindowProcA(hwnd,message,w,l);
}
static void pump(void)
{
    MSG message;
    while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){
        TranslateMessage(&message);DispatchMessageA(&message);
    }
}
void WINAPI RPEntry(void)
{
    char *last=folder,text[256];DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    if(!n||n>=MAX_PATH-20)ExitProcess(2);
    for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
    *last=0;
    lstrcpyA(path,folder);lstrcatA(path,"AVTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
#ifdef RP_AV_PCM
    line("Scope: decoded AAC PCM diagnostic; no sound device output.\r\n");
#else
    line("Scope: GDI and waveOut output; independent host pixel/audio timing analysis required.\r\n");
#endif
    WNDCLASSA cls={0};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleA(NULL);
    cls.hbrBackground=GetStockObject(BLACK_BRUSH);cls.lpszClassName="RPAVTEST";
    if(!RegisterClassA(&cls))ExitProcess(4);
    window=CreateWindowExA(WS_EX_TOPMOST,cls.lpszClassName,"RetroPlayer AV test",WS_POPUP|WS_VISIBLE,
        80,60,384,288,NULL,NULL,cls.hInstance,NULL);
    if(!window)ExitProcess(4);
    UpdateWindow(window);pump();
    line("WINDOW_X=80 WINDOW_Y=60 WIDTH=384 HEIGHT=288\r\n");
    lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
    if(!dll)ExitProcess(5);
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
    APIS(BIND)
    if(failures)ExitProcess(5);
    const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan",
#ifdef RP_AV_PERF
        "--clock-source=perf",
#endif
#ifdef RP_AV_FORMAT
        "--demux=avformat",
#endif
#ifdef RP_AV_FIXED
        "--avcodec-codec=aac_fixed",
#endif
        "--no-media-library","--aout=waveout","--no-waveout-float32","--vout=rpgdi",
        "--no-sub-autodetect-file","--no-spu","--no-osd","--no-video-title-show",
        "--file-caching=1000","--avcodec-threads=2"};
#ifdef RP_AV_PERF
    MMRESULT period=timeBeginPeriod(5);
#endif
    libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
#ifdef RP_AV_PERF
    __typeof__(&libvlc_clock) read_clock=(__typeof__(&libvlc_clock))GetProcAddress(dll,"libvlc_clock");
    unsigned fine_samples=0;
    CHECK(period==TIMERR_NOERROR);CHECK(read_clock!=NULL);
    if(read_clock) for(unsigned tick=0;tick<32;++tick) {
        if(read_clock()%1000) ++fine_samples;
        Sleep(1);
    }
    CHECK(fine_samples>0);
    wsprintfA(text,"CLOCK_SOURCE_REQUEST=perf PERIOD5_RESULT=%u SUB_MS_READS=%u\r\n",period,fine_samples);line(text);
#endif
#ifdef RP_AV_FORMAT
    line("DEMUX_OVERRIDE=avformat (diagnostic comparison)\r\n");
#else
    line("DEMUX_OVERRIDE=none\r\n");
#endif
#ifdef RP_AV_FIXED
    line("AAC_OVERRIDE=aac_fixed (diagnostic comparison)\r\n");
#endif
    if(!instance)ExitProcess(6);
    lstrcpyA(path,folder);lstrcatA(path,"AVLOG.TXT");
    log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(log_file==INVALID_HANDLE_VALUE)ExitProcess(7);
    format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
    InitializeCriticalSection(&log_lock);rp_libvlc_log_set(instance,logger,NULL);
#ifdef RP_AV_LEAD
    const char *files[]={"AV2H264.MP4","AV2HEVC.MKV"};
    line("FIXTURE_LEAD_SECONDS=1 (black/silence before all 40 timed scenes)\r\n");
#else
    const char *files[]={"AVH264.MP4","AVHEVC.MKV"};
#endif
#ifdef RP_AV_PCM
    const unsigned fixture_count=1;
#else
    const unsigned fixture_count=2;
#endif
    for(unsigned i=0;i<fixture_count;++i){
        waveout=amem=gdi=audio_errors=0;
        MEMORYSTATUS memory={0};memory.dwLength=sizeof(memory);GlobalMemoryStatus(&memory);
        DWORD minimum_physical=memory.dwAvailPhys;
        wsprintfA(text,"MEMORY_TOTAL_KB=%lu AVAILABLE_KB=%lu LOAD_PERCENT=%lu\r\n",
            memory.dwTotalPhys/1024,memory.dwAvailPhys/1024,memory.dwMemoryLoad);line(text);
        wsprintfA(text,"FIXTURE=%u FILE=%s STEP=START\r\n",i,files[i]);line(text);
        lstrcpyA(path,folder);lstrcatA(path,files[i]);
        libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
        libvlc_media_player_t *player=media?rp_libvlc_media_player_new_from_media(media):NULL;
        if(media)rp_libvlc_media_release(media);
        if(!player)ExitProcess(6);
        rp_libvlc_media_player_set_hwnd(player,window);
#ifdef RP_AV_PCM
        rp_libvlc_audio_set_callbacks(player,pcm_play,NULL,NULL,NULL,NULL,NULL);
        rp_libvlc_audio_set_format(player,"S16N",22050,1);
#endif
#ifndef RP_AV_PCM
        CHECK(rp_libvlc_audio_set_volume(player,100)==0);
#endif
        CHECK(rp_libvlc_media_player_play(player)==0);
        DWORD before=GetTickCount(),memory_tick=before;libvlc_state_t state;
        do{
            pump();state=rp_libvlc_media_player_get_state(player);
            if(GetTickCount()-memory_tick>=1000){
                GlobalMemoryStatus(&memory);memory_tick=GetTickCount();
                if(memory.dwAvailPhys<minimum_physical)minimum_physical=memory.dwAvailPhys;
            }
            if(state==libvlc_Ended||state==libvlc_Error)break;
            Sleep(10);
        }while(GetTickCount()-before<90000);
        CHECK(state==libvlc_Ended);
#ifdef RP_AV_PCM
        CHECK(amem);
#else
        CHECK(waveout);
#endif
        CHECK(gdi);CHECK(!audio_errors);
        before=GetTickCount();while(GetTickCount()-before<500){pump();Sleep(10);}
        rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);
        wsprintfA(text,"MIN_AVAILABLE_PHYSICAL_KB=%lu\r\n",minimum_physical/1024);line(text);
        wsprintfA(text,"FIXTURE=%u STATE=%u WAVEOUT=%ld RPGDI=%ld AUDIO_ERRORS=%ld\r\n",i,state,waveout,gdi,audio_errors);line(text);
    }
    rp_libvlc_release(instance);
#ifdef RP_AV_PERF
    if(period==TIMERR_NOERROR)timeEndPeriod(5);
#endif
#ifdef RP_AV_PCM
    lstrcpyA(path,folder);lstrcatA(path,"AVPCM.RAW");
    HANDLE pcm_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    CHECK(pcm_file!=INVALID_HANDLE_VALUE);CHECK(decoded_count>0&&!decoded_overflow);
    if(pcm_file!=INVALID_HANDLE_VALUE){
        DWORD count;CHECK(WriteFile(pcm_file,decoded,decoded_count*2,&count,NULL)&&count==decoded_count*2);
        CloseHandle(pcm_file);
    }
    wsprintfA(text,"PCM_SAMPLES=%u OVERFLOW=%u\r\n",decoded_count,decoded_overflow);line(text);
#endif
    DWORD written;CHECK(WriteFile(log_file,log_buffer,log_used,&written,NULL)&&written==log_used);
    CHECK(!log_dropped);DeleteCriticalSection(&log_lock);
    CloseHandle(log_file);FreeLibrary(dll);DestroyWindow(window);
    wsprintfA(text,"CHECKS=%u FAILURES=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",checks,failures,first_failure,failures?"FAIL":"PASS");
    line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
