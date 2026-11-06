/* Decode an independently generated PCM WAV and verify every output byte.
 * This tests demux/decode/audio callbacks, not physical audio hardware. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>

#define API_LIST(X) \
    X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) \
    X(libvlc_media_release) X(libvlc_media_player_new_from_media) \
    X(libvlc_media_player_release) X(libvlc_media_player_play) \
    X(libvlc_media_player_stop) X(libvlc_media_player_get_state) \
    X(libvlc_audio_set_callbacks) X(libvlc_audio_set_format) X(libvlc_log_set)
#define DECLARE(name) static __typeof__(&name) rp_##name;
API_LIST(DECLARE)
static volatile unsigned samples;
static volatile uint32_t actual_hash = 2166136261UL;
static HANDLE report, log_file;
static char folder[MAX_PATH], path[MAX_PATH];
static unsigned checks, failures;
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(e) do { ++checks; if (!(e)) ++failures; } while (0)

static void line(const char *text)
{
    DWORD length=(DWORD)lstrlenA(text), written;
    if (!WriteFile(report,text,length,&written,NULL) || written!=length) ExitProcess(8);
    FlushFileBuffers(report);
}
static void logger(void *opaque, int level, const libvlc_log_t *context,
                   const char *format, va_list args)
{
    DWORD written;
    char text[1024];
    (void)opaque; (void)level; (void)context;
    if(format_log) { format_log(text,sizeof(text)-1,format,args); text[sizeof(text)-1]=0; format=text; }
    WriteFile(log_file,format,(DWORD)lstrlenA(format),&written,NULL);
    WriteFile(log_file,"\r\n",2,&written,NULL);
}
static void audio(void *opaque, const void *data, unsigned count, int64_t pts)
{
    const unsigned char *bytes=data;
    unsigned i;
    uint32_t hash=actual_hash;
    (void)opaque; (void)pts;
    for (i=0; i<count*2; ++i) hash=(hash ^ bytes[i])*16777619UL;
    actual_hash=hash;
    samples+=count;
}
static void put32(unsigned char *p, uint32_t value)
{
    unsigned i;
    for (i=0; i<4; ++i) p[i]=(unsigned char)(value>>(i*8));
}
static uint32_t make_wave(void)
{
    unsigned char header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E',
        'f','m','t',' ',16,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,2,0,16,0,
        'd','a','t','a',0,0,0,0};
    unsigned char block[2000];
    unsigned i, j;
    DWORD written;
    uint32_t hash=2166136261UL;
    HANDLE file;
    put32(header+4,32036); put32(header+24,8000);
    put32(header+28,16000); put32(header+40,32000);
    lstrcpyA(path,folder); lstrcatA(path,"SAMPLE.WAV");
    file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) ExitProcess(9);
    if (!WriteFile(file,header,sizeof(header),&written,NULL) || written!=sizeof(header)) ExitProcess(9);
    for (i=0; i<16000; i+=1000) {
        for (j=0; j<1000; ++j) {
            short sample=(short)(((int)((i+j)%97)-48)*400);
            block[j*2]=(unsigned char)sample;
            block[j*2+1]=(unsigned char)((unsigned short)sample>>8);
        }
        for (j=0; j<sizeof(block); ++j) hash=(hash ^ block[j])*16777619UL;
        if (!WriteFile(file,block,sizeof(block),&written,NULL) || written!=sizeof(block)) ExitProcess(9);
    }
    CloseHandle(file); return hash;
}

void WINAPI RPEntry(void)
{
    HMODULE library;
    libvlc_instance_t *instance;
    libvlc_media_t *media;
    libvlc_media_player_t *player;
    libvlc_state_t state=libvlc_NothingSpecial;
    /* Decode correctness is independent of emulator startup scheduling.
     * Keep strict sample/hash assertions, with enough input clock headroom
     * for loading the larger plugin/filter set on the old guests. */
    const char *args[]={"--ignore-config",
#if RP_USE_CACHE
                        "--plugins-cache","--no-plugins-scan",
#else
                        "--no-plugins-cache",
#endif
                        "--no-video","--no-media-library",
                        "--file-caching=2000"};
    char *p,*last,text[256];
    DWORD length,before;
    uint32_t expected;
    length=GetModuleFileNameA(NULL,folder,MAX_PATH);
    if (!length || length>=MAX_PATH-20) ExitProcess(2);
    last=folder;
    for (p=folder; *p; ++p) if (*p=='\\') last=p+1;
    *last=0;
    lstrcpyA(path,folder); lstrcatA(path,"PCMTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report==INVALID_HANDLE_VALUE) ExitProcess(3);
    line("Scope: PCM WAV decode to memory; physical audio is untested.\r\nSTEP=LOAD\r\n");
    lstrcpyA(path,folder); lstrcatA(path,"LIBVLC.DLL");
    library=LoadLibraryA(path);
    if (!library) { line("LOAD=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(4); }
#define BIND(name) rp_##name=(__typeof__(&name))GetProcAddress(library,#name); CHECK(rp_##name!=NULL);
    API_LIST(BIND)
    format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
    if (failures) { line("EXPORTS=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(5); }
    expected=make_wave();
    line("STEP=CREATE\r\n");
    instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
    if (!instance) { line("CREATE=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(6); }
    lstrcpyA(path,folder); lstrcatA(path,"PCMLOG.TXT");
    log_file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (log_file==INVALID_HANDLE_VALUE) ExitProcess(7);
    rp_libvlc_log_set(instance,logger,NULL);
    lstrcpyA(path,folder); lstrcatA(path,"SAMPLE.WAV");
    media=rp_libvlc_media_new_path(instance,path);
    if (!media) { line("MEDIA=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(6); }
    player=rp_libvlc_media_player_new_from_media(media);
    rp_libvlc_media_release(media);
    if (!player) { line("PLAYER=FAIL\r\nRESULT=FAIL\r\n"); ExitProcess(6); }
    rp_libvlc_audio_set_callbacks(player,audio,NULL,NULL,NULL,NULL,NULL);
    rp_libvlc_audio_set_format(player,"S16N",8000,1);
    line("STEP=PLAY\r\n");
    CHECK(rp_libvlc_media_player_play(player)==0);
    before=GetTickCount();
    while (GetTickCount()-before<30000) {
        state=rp_libvlc_media_player_get_state(player);
        if (state==libvlc_Ended || state==libvlc_Error) break;
        Sleep(10);
    }
    line("STEP=STOP\r\n");
    rp_libvlc_media_player_stop(player);
    CHECK(state==libvlc_Ended);
    CHECK(samples==16000);
    CHECK(actual_hash==expected);
    wsprintfA(text,"STATE=%u SAMPLES=%u EXPECTED=16000\r\nHASH=%08lx EXPECTED_HASH=%08lx\r\n",
        state,samples,(DWORD)actual_hash,(DWORD)expected); line(text);
    rp_libvlc_media_player_release(player);
    rp_libvlc_release(instance);
    CloseHandle(log_file); FreeLibrary(library);
    wsprintfA(text,"CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",checks,failures,failures ? "FAIL" : "PASS");
    line(text); CloseHandle(report); ExitProcess(failures ? 1 : 0);
}
