/* Read-only live guest reports; does not lock out the running test writer. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char folder[MAX_PATH],path[MAX_PATH],text[12000];
static HWND editor;
static void refresh(void)
{
    static const char *files[]={"SUITE.TXT","CRTEST.TXT","DEPTST.TXT","PLOAD.TXT","ZTEST.TXT","SUBTEST.TXT"};
    char *out=text;
    for(unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i) {
        lstrcpyA(out,files[i]);out+=lstrlenA(out);lstrcpyA(out,"\r\n");out+=2;
        lstrcpyA(path,folder);lstrcatA(path,files[i]);
        HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                NULL,OPEN_EXISTING,0,NULL);
        if(file!=INVALID_HANDLE_VALUE) {
            DWORD size=GetFileSize(file,NULL),count=0;
            if(size>1200 && size!=INVALID_FILE_SIZE) SetFilePointer(file,-1200,NULL,FILE_END);
            if(ReadFile(file,out,1200,&count,NULL)) out+=count;
            CloseHandle(file);
        }
        lstrcpyA(out,"\r\n\r\n");out+=4;
    }
    *out=0;
    /* Preserve the user's scroll position when no report changed. */
    static char previous[12000];
    if(lstrcmpA(previous,text)) { lstrcpyA(previous,text);SetWindowTextA(editor,text); }
}
static LRESULT CALLBACK window(HWND handle,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_CREATE) {
        editor=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|
            ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,
            0,0,0,0,handle,NULL,GetModuleHandleA(NULL),NULL);
        SendMessageA(editor,WM_SETFONT,(WPARAM)GetStockObject(SYSTEM_FIXED_FONT),FALSE);
        refresh();SetTimer(handle,1,1000,NULL);return 0;
    }
    if(message==WM_SIZE) { MoveWindow(editor,0,0,LOWORD(lp),HIWORD(lp),TRUE);return 0; }
    if(message==WM_SETFOCUS) { SetFocus(editor);return 0; }
    if(message==WM_TIMER) { refresh();return 0; }
    if(message==WM_DESTROY) { KillTimer(handle,1);PostQuitMessage(0);return 0; }
    return DefWindowProcA(handle,message,wp,lp);
}
void WINAPI RPEntry(void)
{
    char *last=folder;
    DWORD n=GetModuleFileNameA(NULL,folder,MAX_PATH);
    if(!n || n>=MAX_PATH-16) ExitProcess(2);
    for(char *p=folder;*p;++p) if(*p=='\\') last=p+1;
    *last=0;
    WNDCLASSA cls={0};MSG msg;
    cls.lpfnWndProc=window;cls.hInstance=GetModuleHandleA(NULL);
    cls.hCursor=LoadCursorA(NULL,IDC_ARROW);cls.lpszClassName="RPReport";
    if(!RegisterClassA(&cls)) ExitProcess(3);
    HWND handle=CreateWindowA(cls.lpszClassName,"RetroPlayer test reports",WS_OVERLAPPEDWINDOW|
        WS_VISIBLE,10,10,610,440,NULL,NULL,cls.hInstance,NULL);
    if(!handle) ExitProcess(4);
    while(GetMessageA(&msg,NULL,0,0)>0) { TranslateMessage(&msg);DispatchMessageA(&msg); }
    ExitProcess(0);
}
