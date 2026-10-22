#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objidl.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "../src/commands.hpp"

static HWND waitWindow(const wchar_t* cls){
    for(int i=0;i<80;++i){if(HWND w=FindWindowW(cls,nullptr))return w;Sleep(100);}return nullptr;
}
static void click(HWND dialog,int id){SendMessageW(GetDlgItem(dialog,id),BM_CLICK,0,0);}
int wmain(int argc,wchar_t**){
    OleInitialize(nullptr);
    HWND pet=waitWindow(L"YuukiDesktopPetWindow");assert(pet);
    if(argc>1){SendMessageW(pet,WM_CLOSE,0,0);OleUninitialize();return 0;}
    // Native window/menu routes, with real controls, not a duplicate state machine.
    SendMessageW(pet,WM_COMMAND,C_SETTINGS,0);
    HWND settings=waitWindow(L"YuukiSettingsWindow");assert(settings&&IsWindowVisible(settings));
    SetDlgItemInt(settings,201,1,FALSE);SetDlgItemInt(settings,202,1,FALSE);SetDlgItemInt(settings,203,5,FALSE);
    SendMessageW(GetDlgItem(settings,204),BM_SETCHECK,BST_CHECKED,0);
    SendMessageW(GetDlgItem(settings,205),BM_SETCHECK,BST_UNCHECKED,0);
    click(settings,C_APPLY);
    assert(!IsWindowVisible(settings));
    SendMessageW(pet,WM_COMMAND,C_FOCUS,0);
    HWND panel=waitWindow(L"YuukiFocusWindow");assert(panel);
    click(panel,C_CUSTOMFOCUS);
    wchar_t text[100]{};GetWindowTextW(GetDlgItem(panel,C_CUSTOMFOCUS),text,100);assert(std::wstring(text).find(L"1")!=std::wstring::npos);
    click(panel,C_PAUSE);
    GetWindowTextW(GetDlgItem(panel,C_PAUSE),text,100);assert(std::wstring(text)==L"继续");
    click(panel,C_PAUSE);GetWindowTextW(GetDlgItem(panel,C_PAUSE),text,100);assert(std::wstring(text)==L"暂停");
    click(panel,C_STOP);assert(!IsWindowEnabled(GetDlgItem(panel,C_PAUSE)));
    SendMessageW(panel,WM_CLOSE,0,0);assert(!IsWindowVisible(panel));
    // User-facing startup switch writes only this isolated test Windows profile.
    SendMessageW(pet,WM_COMMAND,C_AUTOSTART,0);
    HKEY key=nullptr;assert(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)==ERROR_SUCCESS);
    DWORD bytes=0;assert(RegQueryValueExW(key,L"YuukiDesktopPet",nullptr,nullptr,nullptr,&bytes)==ERROR_SUCCESS&&bytes>2);RegCloseKey(key);
    SendMessageW(pet,WM_COMMAND,C_AUTOSTART,0);
    // Restore release defaults in the isolated runtime profile.
    SendMessageW(pet,WM_COMMAND,C_SETTINGS,0);SetDlgItemInt(settings,201,25,FALSE);SetDlgItemInt(settings,202,5,FALSE);SetDlgItemInt(settings,203,45,FALSE);click(settings,C_APPLY);
    std::puts("Native settings, custom focus, pause/resume/stop, panel close and startup toggle passed.");
    OleUninitialize();return 0;
}
