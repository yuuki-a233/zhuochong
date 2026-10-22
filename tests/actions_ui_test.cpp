#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include "../src/commands.hpp"
#include "../src/animation.hpp"
static std::wstring title(HWND pet){wchar_t s[128]{};GetWindowTextW(pet,s,128);return s;}
int wmain(){
 HWND pet=nullptr;for(int i=0;i<80&&!pet;++i){pet=FindWindowW(L"YuukiDesktopPetWindow",nullptr);Sleep(100);}assert(pet);
 wchar_t data[MAX_PATH];SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,data);
 std::wstring ini=std::wstring(data)+L"\\YuukiDesktopPet\\settings.ini";
 bool autoBefore=GetPrivateProfileIntW(L"Pet",L"AutomaticPoses",1,ini.c_str())!=0;
 bool wanderBefore=GetPrivateProfileIntW(L"Pet",L"Wander",1,ini.c_str())!=0;
 if(autoBefore)SendMessageW(pet,WM_COMMAND,C_AUTOPOSE,0);
 if(wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
 RECT original;GetWindowRect(pet,&original);
 const wchar_t* names[]={L"伸懒腰",L"揉眼打哈欠",L"整理头发",L"左右张望",L"坐着晃脚",L"托腮发呆",L"轻轻拍手",L"原地小跳"};
 for(int i=0;i<8;++i){
  SendMessageW(pet,WM_COMMAND,C_STRETCH+i,0);assert(title(pet).find(names[i])!=std::wstring::npos);
  Sleep(yuuki::actionDuration(static_cast<yuuki::Pose>(int(yuuki::Pose::Stretch)+i))+180);
  assert(title(pet).find(L"待机")!=std::wstring::npos);
  RECT r;GetWindowRect(pet,&r);assert(EqualRect(&r,&original));
 }
 std::puts("All eight action commands returned to idle with unchanged placement.");std::fflush(stdout);
 std::set<std::wstring> responses;
 // Exercise real mouse messages; every click restarts Pat then may queue a response.
 for(int i=0;i<35&&responses.size()<2;++i){
  int x=(original.left+original.right)/2,y=original.top+120;
  SetCursorPos(x,y);Sleep(80);SendMessageW(pet,WM_LBUTTONDOWN,MK_LBUTTON,0);SendMessageW(pet,WM_LBUTTONUP,0,0);
  assert(title(pet).find(L"摸头")!=std::wstring::npos);
  Sleep(1640);auto s=title(pet);if(s.find(L"拍手")!=std::wstring::npos||s.find(L"小跳")!=std::wstring::npos)responses.insert(s);
 }
 assert(responses.size()==2);
 // Interrupt queued reactions and ensure they cannot leak into focus mode.
 SendMessageW(pet,WM_COMMAND,C_CUSTOMFOCUS,0);Sleep(4000);assert(title(pet).find(L"读书")!=std::wstring::npos);
 SendMessageW(pet,WM_COMMAND,C_STOP,0);
 if(autoBefore)SendMessageW(pet,WM_COMMAND,C_AUTOPOSE,0);
 if(wanderBefore)SendMessageW(pet,WM_COMMAND,C_WANDER,0);
 std::puts("Mouse interactions triggered both clap and hop; focus interruption cleared pending responses.");return 0;
}
