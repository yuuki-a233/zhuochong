#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include "../src/commands.hpp"

static void dragTo(HWND pet,int x,int y){
    RECT r;GetWindowRect(pet,&r);int dx=(r.right-r.left)/2,dy=120;
    SetCursorPos(r.left+dx,r.top+dy);Sleep(80);SendMessageW(pet,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(dx,dy));
    SetCursorPos(x+dx,y+dy);Sleep(80);SendMessageW(pet,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(dx,dy));
    SendMessageW(pet,WM_LBUTTONUP,0,MAKELPARAM(dx,dy));
    RECT moved;GetWindowRect(pet,&moved);
    if(moved.left!=x||moved.top!=y){std::printf("Drag expected (%d,%d), observed (%ld,%ld).\n",x,y,moved.left,moved.top);std::fflush(stdout);}
    assert(moved.left==x&&moved.top==y);
}
int wmain(int argc,wchar_t**){
    HWND pet=nullptr;for(int i=0;i<80&&!pet;++i){pet=FindWindowW(L"YuukiDesktopPetWindow",nullptr);Sleep(100);}assert(pet);
    wchar_t data[MAX_PATH];SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,data);
    std::wstring ini=std::wstring(data)+L"\\YuukiDesktopPet\\settings.ini";
    bool autoBefore=GetPrivateProfileIntW(L"Pet",L"AutomaticPoses",1,ini.c_str())!=0;
    bool wanderBefore=GetPrivateProfileIntW(L"Pet",L"Wander",1,ini.c_str())!=0;
    if(argc>1){
        if(wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
        dragTo(pet,340,180);dragTo(pet,500,-72);dragTo(pet,340,180);
        if(wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
        std::puts("Exact free placement including top-edge placement passed.");return 0;
    }
    if(!autoBefore)SendMessageW(pet,WM_COMMAND,C_AUTOPOSE,0);
    if(!wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
    RECT original;GetWindowRect(pet,&original);
    dragTo(pet,340,180);SetCursorPos(20,20);
    assert(GetPrivateProfileIntW(L"Pet",L"X",0,ini.c_str())==340);
    assert(GetPrivateProfileIntW(L"Pet",L"Y",0,ini.c_str())==180);
    std::puts("Free placement and saved position passed.");std::fflush(stdout);
    std::set<std::wstring> states;bool walked=false,manualStarted=false,manualResumed=false;
    ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<55000){
        auto elapsed=GetTickCount64()-start;
        if(elapsed>=14000&&!manualStarted){SendMessageW(pet,WM_COMMAND,C_READ,0);manualStarted=true;}
        wchar_t title[100]{};GetWindowTextW(pet,title,100);std::wstring state=title;states.insert(state);
        if(state.find(L"散步")!=std::wstring::npos)walked=true;
        if(manualStarted&&elapsed>23000&&state.find(L"读书")==std::wstring::npos)manualResumed=true;
        RECT r;GetWindowRect(pet,&r);assert(r.top==180);assert(r.left>=240&&r.left<=440);
        Sleep(100);
    }
    assert(states.size()>=3&&walked&&manualResumed);
    std::printf("Observed %zu automatic states; walking and automatic resume passed.\n",states.size());std::fflush(stdout);
    SendMessageW(pet,WM_COMMAND,C_WANDER,0); // stop movement before exact-pixel drag assertions
    // Body may reach the top edge; the invisible dialogue margin is allowed offscreen.
    dragTo(pet,500,-72);dragTo(pet,original.left,original.top);
    SendMessageW(pet,WM_COMMAND,C_WANDER,0);
    if(!autoBefore)SendMessageW(pet,WM_COMMAND,C_AUTOPOSE,0);
    if(!wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
    std::printf("Observed %zu automatic states; walking retained vertical placement and manual reading resumed automatically.\n",states.size());
    return 0;
}
