/* Observe subtitle appearance, movement and removal on an unchanged video
 * through actual GDI pixels. A stale prepared bitmap must fail this test. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <vlc/vlc.h>
#define APIS(X) X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) \
 X(libvlc_media_release) X(libvlc_media_add_option) X(libvlc_media_player_new_from_media) \
 X(libvlc_media_player_release) X(libvlc_media_player_play) X(libvlc_media_player_stop) \
 X(libvlc_media_player_get_time) X(libvlc_media_player_get_state) \
 X(libvlc_media_player_set_hwnd) X(libvlc_log_set)
#define DECLARE(n) static __typeof__(&n) rp_##n;
APIS(DECLARE)
static HANDLE report;
static unsigned checks, failures;
static volatile LONG gdi, ass;
static char folder[MAX_PATH], path[MAX_PATH], option[MAX_PATH+32];
static int (__cdecl *format_log)(char *,size_t,const char *,va_list);
#define CHECK(x) do { ++checks; if (!(x)) ++failures; } while (0)
static void line(const char *text)
{
    DWORD n=lstrlenA(text),written;
    if(!WriteFile(report,text,n,&written,NULL)||written!=n)ExitProcess(8);
    FlushFileBuffers(report);
}
static BOOL contains(const char *text,const char *part)
{
    for(;*text;++text){const char *a=text,*b=part;while(*b&&*a==*b){++a;++b;}if(!*b)return TRUE;}
    return FALSE;
}
static void logger(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args)
{
    char text[1024];(void)opaque;(void)level;(void)ctx;
    if(format_log){format_log(text,sizeof(text)-1,fmt,args);text[sizeof(text)-1]=0;fmt=text;}
    if(contains(fmt,"using vout display module \"RPGDI\""))gdi=1;
    if(contains(fmt,"using spu decoder module \"libass\""))ass=1;
}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l)
{
    if(message==WM_CLOSE)return 0;
    return DefWindowProcA(window,message,w,l);
}
static void pump(void)
{
    MSG message;
    while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){
        TranslateMessage(&message);DispatchMessageA(&message);
    }
}
static unsigned luminance(HDC dc,int x,int y)
{
    COLORREF color=GetPixel(dc,x,y);
    if(color==CLR_INVALID)return 1000;
    return (GetRValue(color)+GetGValue(color)+GetBValue(color))/3;
}
static BOOL pixel_match(unsigned actual,unsigned expected)
{
    return actual>expected ? actual-expected<=8 : expected-actual<=8;
}
void WINAPI RPEntry(void)
{
    DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);char *last=folder,text[160];
    if(!n||n>=MAX_PATH-24)ExitProcess(2);
    for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;
    *last=0;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    lstrcpyA(path,folder);lstrcatA(path,"GDISUB.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
    line("Scope: actual GDI subtitle pixel transitions over constant video; no sound output.\r\n");
    /* Owned vector subtitles put a white square at two distinct locations.
     * Blank intervals require removal, not just appearance of an overlay. */
    static const char subtitles[]=
        "[Script Info]\r\nScriptType: v4.00+\r\nPlayResX: 320\r\nPlayResY: 240\r\n"
        "[V4+ Styles]\r\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
        "Style: Default,DejaVu Sans,22,&H00FFFFFF,&H00000000,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\r\n"
        "[Events]\r\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n"
        "Dialogue: 0,0:00:02.00,0:00:05.00,Default,,0,0,0,,{\\an7\\pos(64,96)\\p1}m 0 0 l 64 0 64 48 0 48{\\p0}\r\n"
        "Dialogue: 0,0:00:07.00,0:00:10.00,Default,,0,0,0,,{\\an7\\pos(192,96)\\p1}m 0 0 l 64 0 64 48 0 48{\\p0}\r\n";
    lstrcpyA(path,folder);lstrcatA(path,"GDISUB.ASS");
    HANDLE file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(file==INVALID_HANDLE_VALUE)ExitProcess(4);
    CHECK(WriteFile(file,subtitles,sizeof(subtitles)-1,&n,NULL)&&n==sizeof(subtitles)-1);CloseHandle(file);
    WNDCLASSA cls={0};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleA(NULL);
    cls.hbrBackground=GetStockObject(BLACK_BRUSH);cls.lpszClassName="RPGDISUB";
    if(!RegisterClassA(&cls))ExitProcess(4);
    HWND window=CreateWindowExA(WS_EX_TOPMOST,cls.lpszClassName,"RetroPlayer subtitle test",
        WS_POPUP|WS_VISIBLE,80,60,320,240,NULL,NULL,cls.hInstance,NULL);
    if(!window)ExitProcess(4);
    UpdateWindow(window);pump();
    lstrcpyA(path,folder);lstrcatA(path,"LIBVLC.DLL");HMODULE dll=LoadLibraryA(path);
    if(!dll)ExitProcess(5);
#define BIND(n) rp_##n=(__typeof__(&n))GetProcAddress(dll,#n);CHECK(rp_##n!=NULL);
    APIS(BIND)
    if(failures)ExitProcess(5);
    format_log=(__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"),"_vsnprintf");
    CHECK(format_log!=NULL);
    const char *args[]={"--ignore-config","--plugins-cache","--no-plugins-scan",
        "--no-media-library","--no-audio","--no-sub-autodetect-file","--no-osd",
        "--no-video-title-show","--no-drop-late-frames","--no-skip-frames",
        "--avcodec-threads=2","--vout=rpgdi","--codec=libass,any","--file-caching=1000"};
    libvlc_instance_t *instance=rp_libvlc_new(sizeof(args)/sizeof(args[0]),args);
    if(!instance)ExitProcess(6);
    rp_libvlc_log_set(instance,logger,NULL);
    lstrcpyA(path,folder);lstrcatA(path,"SUBS.MP4");
    libvlc_media_t *media=rp_libvlc_media_new_path(instance,path);
    if(!media)ExitProcess(6);
    lstrcpyA(option,":sub-file=");lstrcatA(option,folder);lstrcatA(option,"GDISUB.ASS");
    rp_libvlc_media_add_option(media,option);
    libvlc_media_player_t *player=rp_libvlc_media_player_new_from_media(media);
    rp_libvlc_media_release(media);if(!player)ExitProcess(6);
    rp_libvlc_media_player_set_hwnd(player,window);
    CHECK(rp_libvlc_media_player_play(player)==0);
    const unsigned starts[]={500,2500,5500,7500,10500};
    unsigned phase=0,background=1000;DWORD start=GetTickCount();
    while(phase<5&&GetTickCount()-start<45000){
        pump();libvlc_time_t time=rp_libvlc_media_player_get_time(player);
        if(rp_libvlc_media_player_get_state(player)==libvlc_Error)break;
        if(time>=starts[phase]){
            HDC dc=GetDC(window);
            unsigned left=dc?luminance(dc,96,120):1000;
            unsigned right=dc?luminance(dc,224,120):1000;
            if(dc)ReleaseDC(window,dc);
            /* The supplied low-color displays map the solid blue fixture
             * differently (42 on NT, 85 on Win95). Establish its rendered
             * baseline, rejecting both a cleared black window and white,
             * then require the exact background to return after overlays. */
            BOOL expected=phase==0 ? left>=20&&left<=128&&pixel_match(left,right) :
                          phase==1 ? left>=200&&left<256&&pixel_match(right,background) :
                          phase==3 ? right>=200&&right<256&&pixel_match(left,background) :
                          pixel_match(left,background)&&pixel_match(right,background);
            if(phase==0&&expected)background=left;
            if(expected||time>starts[phase]+1200){
                CHECK(expected);wsprintfA(text,"PHASE=%u MEDIA_MS=%lu LEFT=%u RIGHT=%u RESULT=%s\r\n",
                    phase,(unsigned long)time,left,right,expected?"PASS":"FAIL");line(text);++phase;
            }
        }
        Sleep(20);
    }
    CHECK(phase==5);CHECK(gdi==1);CHECK(ass==1);
    rp_libvlc_media_player_stop(player);rp_libvlc_media_player_release(player);
    rp_libvlc_release(instance);FreeLibrary(dll);DestroyWindow(window);
    lstrcpyA(path,folder);lstrcatA(path,"GDISUB.ASS");CHECK(DeleteFileA(path));
    wsprintfA(text,"GDI=%lu LIBASS=%lu CHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",
        (unsigned long)gdi,(unsigned long)ass,checks,failures,failures?"FAIL":"PASS");
    line(text);CloseHandle(report);ExitProcess(failures?1:0);
}
