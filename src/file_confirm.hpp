#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace yuuki {
struct FileConfirmation {bool done=false,accepted=false;std::vector<std::wstring> files;};
inline LRESULT CALLBACK fileConfirmProc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    auto* state=reinterpret_cast<FileConfirmation*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE){
        auto* create=reinterpret_cast<CREATESTRUCTW*>(lp);
        state=static_cast<FileConfirmation*>(create->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto add=[&](DWORD ex,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){
            HWND child=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;
        };
        std::wstring title=L"让由岐吃掉这 "+std::to_wstring(state->files.size())+L" 个文件吗？\r\n确认后送入 Windows 回收站，可从回收站找回。";
        add(0,L"STATIC",title.c_str(),0,18,16,520,50,0);
        std::wstring list;
        for(const auto& file:state->files)list+=file+L"\r\n";
        add(WS_EX_CLIENTEDGE,L"EDIT",list.c_str(),ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP,18,78,520,160,10);
        add(0,L"BUTTON",L"送入回收站",BS_PUSHBUTTON|WS_TABSTOP,286,260,120,34,IDYES);
        HWND cancel=add(0,L"BUTTON",L"取消",BS_DEFPUSHBUTTON|WS_TABSTOP,418,260,120,34,IDCANCEL);
        SetFocus(cancel);return 0;
    }
    if(msg==WM_COMMAND && state && (LOWORD(wp)==IDYES||LOWORD(wp)==IDCANCEL)){
        state->accepted=LOWORD(wp)==IDYES;state->done=true;return 0;
    }
    if(msg==WM_CLOSE && state){state->done=true;return 0;}
    return DefWindowProcW(window,msg,wp,lp);
}
inline bool confirmFiles(HWND owner,const std::vector<std::wstring>& files) {
    static bool registered=false;
    if(!registered){
        WNDCLASSW wc{};wc.lpfnWndProc=fileConfirmProc;wc.hInstance=GetModuleHandleW(nullptr);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"YuukiFileConfirmation";
        registered=RegisterClassW(&wc)!=0;
    }
    if(!registered)return false;
    FileConfirmation state;state.files=files;
    MONITORINFO mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&mi);
    RECT bounds{0,0,556,314};AdjustWindowRect(&bounds,WS_CAPTION|WS_SYSMENU,FALSE);
    int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
    HWND dialog=CreateWindowExW(WS_EX_DLGMODALFRAME,L"YuukiFileConfirmation",L"由岐 · 确认吃文件",WS_CAPTION|WS_SYSMENU,
        mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-w)/2,mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-h)/2,w,h,owner,nullptr,GetModuleHandleW(nullptr),&state);
    if(!dialog)return false;
    EnableWindow(owner,FALSE);ShowWindow(dialog,SW_SHOW);SetForegroundWindow(dialog);
    MSG msg{};bool quit=false;
    while(!state.done){
        int result=GetMessageW(&msg,nullptr,0,0);
        if(result<=0){quit=result==0;break;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){state.done=true;break;}
        if(!IsDialogMessageW(dialog,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    if(IsWindow(dialog))DestroyWindow(dialog);
    if(IsWindow(owner))EnableWindow(owner,TRUE);
    if(quit)PostQuitMessage(int(msg.wParam));
    return !quit&&state.accepted;
}
}
