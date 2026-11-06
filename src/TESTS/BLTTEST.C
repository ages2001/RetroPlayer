/* Compare actual legacy GDI scaling paths. Timing is diagnostic, not a speed guarantee. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static unsigned char rgb[64*48*3], indexed[32*48];
static struct { BITMAPINFOHEADER header; RGBQUAD colors[16]; } bitmap;
static HANDLE report;
static HWND window;
static unsigned errors;
static LONGLONG frequency;
static void line(const char *s) { DWORD n; WriteFile(report,s,lstrlenA(s),&n,NULL); }
static DWORD WINAPI watchdog(void *unused) { (void)unused; Sleep(30000);line("WATCHDOG=TIMEOUT\r\n");ExitProcess(9);return 0; }
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l) { return DefWindowProcA(hwnd,message,w,l); }
static void measure(unsigned bits, unsigned run)
{
    DWORD times[48],sum=0,first; char text[192];
    bitmap.header.biBitCount=(WORD)bits; bitmap.header.biClrUsed=bits==4?16:0;
    for(unsigned frame=0;frame<48;++frame) {
        for(unsigned y=0;y<48;++y) for(unsigned x=0;x<64;++x) {
            unsigned value=(x<8 || (x<16 && (frame&1)))?255:0;
            unsigned color=x>=16 ? 1+(x/8)%7 : (value?15:0);
            const RGBQUAD *c=&bitmap.colors[color];
            unsigned offset=(y*64+x)*3;
            rgb[offset]=c->rgbBlue; rgb[offset+1]=c->rgbGreen; rgb[offset+2]=c->rgbRed;
            if(!(x&1)) indexed[y*32+x/2]=(unsigned char)(color<<4);
            else indexed[y*32+x/2]|=(unsigned char)color;
        }
        LARGE_INTEGER begin,end;
        QueryPerformanceCounter(&begin);
        HDC dc=GetDC(window);
        if(!dc) { ++errors; return; }
        SetStretchBltMode(dc,COLORONCOLOR);
        int result=StretchDIBits(dc,0,0,384,288,0,0,64,48,bits==4?indexed:rgb,(BITMAPINFO *)&bitmap,DIB_RGB_COLORS,SRCCOPY);
        COLORREF one=GetPixel(dc,24,144),two=GetPixel(dc,72,144);
        ReleaseDC(window,dc);
        QueryPerformanceCounter(&end);
        if(result==0 || result==(int)GDI_ERROR || one!=RGB(255,255,255) || two!=((frame&1)?RGB(255,255,255):RGB(0,0,0))) ++errors;
        times[frame]=(DWORD)((end.QuadPart-begin.QuadPart)*1000000/frequency);sum+=times[frame];
        MSG msg; while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg);DispatchMessageA(&msg); }
    }
    first=times[0];
    for(unsigned i=1;i<48;++i) { DWORD v=times[i];unsigned j=i;while(j && times[j-1]>v) { times[j]=times[j-1];--j; } times[j]=v; }
    wsprintfA(text,"BPP=%u RUN=%u FIRST_US=%lu MEDIAN_US=%lu P95_US=%lu MAX_US=%lu MEAN_US=%lu\r\n",bits,run,first,times[24],times[45],times[47],sum/48);line(text);
}
void WINAPI RPEntry(void)
{
    char path[MAX_PATH],*last=path,text[160];DWORD id,n=GetModuleFileNameA(NULL,path,MAX_PATH);
    if(!n || n>=MAX_PATH-16) ExitProcess(2);
    for(char *p=path;*p;++p) if(*p=='\\') last=p+1;
    *last=0;lstrcatA(path,"BLTTEST.TXT");
    report=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(3);
    LARGE_INTEGER freq;if(!QueryPerformanceFrequency(&freq) || !freq.QuadPart)ExitProcess(4);frequency=freq.QuadPart;
    HANDLE guard=CreateThread(NULL,0,watchdog,NULL,0,&id);if(!guard)ExitProcess(4);CloseHandle(guard);
    WNDCLASSA cls={0};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleA(NULL);cls.lpszClassName="RPBLTTEST";
    if(!RegisterClassA(&cls))ExitProcess(4);
    window=CreateWindowExA(WS_EX_TOPMOST,cls.lpszClassName,"RetroPlayer GDI diagnostic",WS_POPUP|WS_VISIBLE,80,60,384,288,NULL,NULL,cls.hInstance,NULL);if(!window)ExitProcess(4);
    UpdateWindow(window);
    HDC dc=GetDC(window);if(!dc)ExitProcess(4);
    wsprintfA(text,"DEVICE_BPP=%d PLANES=%d COLORS=%d\r\n",GetDeviceCaps(dc,BITSPIXEL),GetDeviceCaps(dc,PLANES),GetDeviceCaps(dc,NUMCOLORS));line(text);ReleaseDC(window,dc);
    for(unsigned i=0;i<16;++i) {
        bitmap.colors[i].rgbRed=(i&1)?(i&8?255:128):0;
        bitmap.colors[i].rgbGreen=(i&2)?(i&8?255:128):0;
        bitmap.colors[i].rgbBlue=(i&4)?(i&8?255:128):0;
    }
    bitmap.colors[7].rgbRed=bitmap.colors[7].rgbGreen=bitmap.colors[7].rgbBlue=192;
    bitmap.colors[8].rgbRed=bitmap.colors[8].rgbGreen=bitmap.colors[8].rgbBlue=128;
    bitmap.header.biSize=sizeof(BITMAPINFOHEADER);bitmap.header.biWidth=64;bitmap.header.biHeight=48;
    bitmap.header.biPlanes=1;bitmap.header.biCompression=BI_RGB;
    measure(4,0);measure(24,0);measure(24,1);measure(4,1);
    DestroyWindow(window);wsprintfA(text,"DRAW_ERRORS=%u\r\n",errors);line(text);CloseHandle(report);ExitProcess(errors?1:0);
}
