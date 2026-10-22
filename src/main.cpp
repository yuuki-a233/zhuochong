#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "core.hpp"
#include "animation.hpp"
#include "commands.hpp"
#include "recycle.hpp"
#include "file_confirm.hpp"

using namespace Gdiplus;
static const wchar_t* PET_CLASS = L"YuukiDesktopPetWindow";
static constexpr int CELL_W = 320, CELL_H = 420, BUBBLE_H = 72;
static constexpr UINT TRAY_MESSAGE = WM_APP + 1, RESTORE_PET = WM_APP + 2;
using yuuki::Pose;
static HINSTANCE appInstance;
static HWND petWindow, focusWindow, focusLabel, pauseButton,settingsWindow;
static HWND focusEdit,breakEdit,reminderEdit,startupCheck,reminderCheck,affectionLabel;
static std::unique_ptr<Bitmap> atlas;
static IStream* atlasStream = nullptr;
static HICON petIcon;
static NOTIFYICONDATAW tray{};
static UINT taskbarCreated;
static std::wstring settingsPath, message;
static std::vector<BYTE> renderedAlpha;
static yuuki::Countdown focus;
static yuuki::Affection affection;
static std::mt19937 randomEngine{std::random_device{}()};
static bool wandering=true, automaticPoses=true, quiet=false, reminders=true, visible=true;
static bool mouseDown=false, dragging=false, menuOpen=false, pausedPose=false;
static bool focusIsBreak=false;
static bool modalBusy=false,dragHover=false,oleReady=false;
static int petSize=220, appX=0, appY=0, direction=1,anchorX=0;
static int focusMinutes=25,breakMinutes=5,reminderMinutes=45;
static POINT downMouse{}, downWindow{};
static Pose pose=Pose::Idle;
static Pose followup=Pose::Idle,lastReaction=Pose::Idle;
static yuuki::ActionDeck dailyActions;
static ULONGLONG poseSince=0, poseUntil=0, nextAction=0, lastTick=0;
static ULONGLONG messageUntil=0, nextReminder=0, lastTipUpdate=0;
static ULONGLONG nextWalk=0;
static double movementRemainder=0;
static int localDay(){SYSTEMTIME t{};GetLocalTime(&t);return t.wYear*10000+t.wMonth*100+t.wDay;}
static ULONGLONG reminderInterval(){return ULONGLONG(reminderMinutes)*60*1000;}
static const wchar_t* affectionStage(){return affection.value<40?L"熟悉":affection.value<70?L"亲近":L"默契";}
static void showSettings();
static bool autoStartEnabled(){
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;
    DWORD size=0,type=0;LONG result=RegQueryValueExW(key,L"YuukiDesktopPet",nullptr,&type,nullptr,&size);RegCloseKey(key);
    return result==ERROR_SUCCESS&&type==REG_SZ&&size>2;
}
static bool setAutoStart(bool enabled){
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LONG result;
    if(enabled){
        std::vector<wchar_t> path(32768);DWORD size=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
        if(!size||size>=path.size()){RegCloseKey(key);return false;}
        std::wstring value=L"\""+std::wstring(path.data(),size)+L"\"";
        result=RegSetValueExW(key,L"YuukiDesktopPet",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));
    }else{result=RegDeleteValueW(key,L"YuukiDesktopPet");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}
    RegCloseKey(key);return result==ERROR_SUCCESS;
}

static int roll(int low, int high) {
    return std::uniform_int_distribution<int>(low, high)(randomEngine);
}
static int width() { return int(std::lround(double(petSize)*CELL_W/CELL_H)); }
static int height() { return petSize+BUBBLE_H; }
static RECT workArea(bool primary=false) {
    POINT p{appX+width()/2,appY+height()/2};
    HMONITOR monitor=MonitorFromPoint(primary?POINT{0,0}:p,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{}; info.cbSize=sizeof(info);
    GetMonitorInfoW(monitor,&info);
    return info.rcWork;
}
static void clampPosition() {
    RECT r=workArea();
    appX=yuuki::clampAxis(appX,width(),r.left,r.right);
    appY=yuuki::clampAxis(appY,height(),r.top-BUBBLE_H,r.bottom);
}
static void saveSettings() {
    auto write=[](const wchar_t* name,int value){
        WritePrivateProfileStringW(L"Pet",name,std::to_wstring(value).c_str(),settingsPath.c_str());
    };
    write(L"Size",petSize); write(L"X",appX); write(L"Y",appY);
    write(L"Wander",wandering); write(L"Quiet",quiet); write(L"Reminders",reminders);
    write(L"AutomaticPoses",automaticPoses);
    write(L"FocusMinutes",focusMinutes);write(L"BreakMinutes",breakMinutes);write(L"ReminderMinutes",reminderMinutes);
    write(L"Affection",affection.value);write(L"AffectionEarned",affection.earned);write(L"AffectionDay",affection.day);
}
static void readSettings() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,0,path)))
        GetTempPathW(MAX_PATH,path);
    settingsPath=std::wstring(path)+L"\\YuukiDesktopPet";
    CreateDirectoryW(settingsPath.c_str(),nullptr);
    settingsPath+=L"\\settings.ini";
    auto read=[](const wchar_t* name,int fallback){return int(GetPrivateProfileIntW(L"Pet",name,fallback,settingsPath.c_str()));};
    petSize=std::clamp(read(L"Size",220),140,360);
    wandering=read(L"Wander",1)!=0; quiet=read(L"Quiet",0)!=0; reminders=read(L"Reminders",1)!=0;
    automaticPoses=read(L"AutomaticPoses",1)!=0;
    focusMinutes=std::clamp(read(L"FocusMinutes",25),1,180);breakMinutes=std::clamp(read(L"BreakMinutes",5),1,60);
    reminderMinutes=std::clamp(read(L"ReminderMinutes",45),5,180);
    affection.value=read(L"Affection",20);affection.earned=read(L"AffectionEarned",0);affection.day=read(L"AffectionDay",0);affection.refresh(localDay());
    RECT r=workArea(true);
    appX=read(L"X",r.right-width()-80); appY=read(L"Y",r.bottom-height());
    clampPosition();
    anchorX=appX;
}
static std::wstring clockText() {
    auto seconds=(focus.remaining(GetTickCount64())+999)/1000;
    wchar_t text[32];
    swprintf(text,32,L"%02llu:%02llu",seconds/60,seconds%60);
    return text;
}
static void say(const wchar_t* text,ULONGLONG duration=4500) {
    if(quiet)return;
    message=text; messageUntil=GetTickCount64()+duration;
}
static void setPose(Pose value,ULONGLONG duration=0) {
    followup=Pose::Idle; // Interruptions discard any queued interaction response.
    pose=value; poseSince=GetTickCount64(); poseUntil=duration?poseSince+duration:0;
    const wchar_t* name=L"待机";
    switch(value){
    case Pose::Walk:name=L"散步";break;case Pose::Greet:name=L"打招呼";break;
    case Pose::Read:name=L"读书";break;case Pose::Sleep:name=L"打瞌睡";break;
    case Pose::Grab:name=L"拖动";break;case Pose::Pat:name=L"摸头";break;
    case Pose::Eat:name=L"吃文件";break;case Pose::Anticipate:name=L"准备接文件";break;
    case Pose::Stretch:name=L"伸懒腰";break;case Pose::Yawn:name=L"揉眼打哈欠";break;
    case Pose::Hair:name=L"整理头发";break;case Pose::Look:name=L"左右张望";break;
    case Pose::Swing:name=L"坐着晃脚";break;case Pose::Think:name=L"托腮发呆";break;
    case Pose::Clap:name=L"轻轻拍手";break;case Pose::Hop:name=L"原地小跳";break;
    default:break;
    }
    if(petWindow)SetWindowTextW(petWindow,(std::wstring(L"由岐桌宠 · ")+name).c_str());
}
static void notify(const wchar_t* title,const wchar_t* text) {
    if (quiet) return;
    tray.uFlags=NIF_INFO;
    lstrcpynW(tray.szInfoTitle,title,ARRAYSIZE(tray.szInfoTitle));
    lstrcpynW(tray.szInfo,text,ARRAYSIZE(tray.szInfo));
    tray.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
    Shell_NotifyIconW(NIM_MODIFY,&tray);
}
static void addTray() {
    tray={}; tray.cbSize=sizeof(tray); tray.hWnd=petWindow; tray.uID=1;
    tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; tray.uCallbackMessage=TRAY_MESSAGE; tray.hIcon=petIcon;
    lstrcpynW(tray.szTip,L"由岐桌宠 · 右键打开菜单",ARRAYSIZE(tray.szTip));
    Shell_NotifyIconW(NIM_ADD,&tray);
    tray.uVersion=NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION,&tray);
}
static void updateTip() {
    std::wstring tip=L"由岐桌宠";
    if(focus.active()) tip+=(focusIsBreak?L" · 休息 ":L" · 专注 ")+clockText()+(focus.paused()?L"（暂停）":L"");
    else tip+=L" · 右键打开菜单";
    tray.uFlags=NIF_TIP; lstrcpynW(tray.szTip,tip.c_str(),ARRAYSIZE(tray.szTip));
    Shell_NotifyIconW(NIM_MODIFY,&tray);
}
static void roundedRect(Graphics& g,const RectF& r,float radius,Color color) {
    GraphicsPath path;
    float d=radius*2;
    path.AddArc(r.X,r.Y,d,d,180,90); path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
    path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90); path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);
    path.CloseFigure(); SolidBrush brush(color); g.FillPath(&brush,&path);
}
static void render() {
    if(!atlas || !visible) return;
    const int w=width(),h=height();
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h; info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr; HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP dib=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!dib || !pixels) { if(dib) DeleteObject(dib); DeleteDC(dc); ReleaseDC(nullptr,screen); return; }
    HGDIOBJ old=SelectObject(dc,dib);
    {
        Bitmap surface(w,h,w*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels));
        Graphics g(&surface); g.Clear(Color(0,0,0,0));
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        ULONGLONG elapsed=GetTickCount64()-poseSince;
        int row=0,col=0;
        if(yuuki::actionRow(pose)>=0) {row=yuuki::actionRow(pose);col=yuuki::actionFrame(pose,elapsed);}
        else if(pose==Pose::Walk) { row=1; col=int(elapsed/145)%4; }
        else if(pose==Pose::Greet || pose==Pose::Grab) { row=2; col=0; }
        else if(pose==Pose::Read) { row=2; col=1; }
        else if(pose==Pose::Sleep) { row=2; col=2; }
        else if(pose==Pose::Pat) {
            row=3;int frame=std::min(3,int(elapsed/300));
            static const int shy[]={0,1,0,3},familiar[]={0,1,2,3},close[]={1,2,1,2};
            col=(affection.value<40?shy:affection.value<70?familiar:close)[frame];
        }
        else if(pose==Pose::Eat) { row=4; col=std::min(3,int(elapsed/450)); }
        else if(pose==Pose::Anticipate) { row=4; col=0; }
        else {
            int t=int(elapsed%4800);
            col=t<4200?0:t<4320?1:t<4440?2:t<4560?1:3;
        }
        // Keep the head and baseline registered across animation frames.
        int bounce=pose==Pose::Grab?-5:0;
        if(pose==Pose::Idle||pose==Pose::Read||pose==Pose::Sleep)
            bounce+=int(std::lround(std::sin(double(elapsed)*6.283185307/3600.0)));
        auto saved=g.Save();
        if(pose==Pose::Walk && direction<0) { g.TranslateTransform(float(w),0); g.ScaleTransform(-1,1); }
        g.DrawImage(atlas.get(),Rect(0,BUBBLE_H+bounce,w,petSize),col*CELL_W,row*CELL_H,CELL_W,CELL_H,UnitPixel);
        g.Restore(saved);
        std::wstring bubble;
        if(GetTickCount64()<messageUntil) bubble=message;
        else if(focus.active()) bubble=(focusIsBreak?L"休息 ":L"专注 ")+clockText()+(focus.paused()?L" · 暂停":L"");
        if(!bubble.empty()) {
            roundedRect(g,RectF(2,4,float(w-4),56),12,Color(245,255,252,248));
            FontFamily family(L"Microsoft YaHei UI");
            const FontFamily* chosen=family.GetLastStatus()==Ok?&family:FontFamily::GenericSansSerif();
            Font font(chosen,w<145?11.0f:12.0f,FontStyleRegular,UnitPixel);
            SolidBrush ink(Color(255,81,58,73)); StringFormat format;
            format.SetAlignment(StringAlignmentCenter); format.SetLineAlignment(StringAlignmentCenter);
            format.SetFormatFlags(StringFormatFlagsLineLimit);
            g.DrawString(bubble.c_str(),-1,&font,RectF(9,7,float(w-18),49),&format,&ink);
        }
        g.Flush(FlushIntentionSync);
        const BYTE* data=static_cast<BYTE*>(pixels);
        renderedAlpha.resize(w*h);
        for(int i=0;i<w*h;++i) renderedAlpha[i]=data[i*4+3];
    }
    POINT position{appX,appY},source{0,0}; SIZE size{w,h}; BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(petWindow,screen,&position,&size,dc,&source,0,&blend,ULW_ALPHA);
    SelectObject(dc,old); DeleteObject(dib); DeleteDC(dc); ReleaseDC(nullptr,screen);
}
static void showPet() {
    visible=true; clampPosition(); ShowWindow(petWindow,SW_SHOWNOACTIVATE); render();
}
static void startFocus(int minutes,bool isBreak=false) {
    focus.start(GetTickCount64(),ULONGLONG(minutes)*60*1000); focusIsBreak=isBreak;
    pausedPose=false; setPose(isBreak?Pose::Idle:Pose::Read); nextAction=GetTickCount64()+10000;
    say(isBreak?L"歇一会儿，看看远处。":L"开始吧，我陪你读一会儿。",4000);
    updateTip(); render();
}
static void focusUi() {
    if(!focusWindow) return;
    std::wstring label;
    if(focus.active()) label=(focusIsBreak?L"休息时间\r\n":L"专注时间\r\n")+clockText()+(focus.paused()?L"  已暂停":L"");
    else label=L"选一段时间，慢慢把事情做好。\r\n提醒时间到后，可自行开始休息。";
    SetWindowTextW(focusLabel,label.c_str());
    SetWindowTextW(pauseButton,focus.paused()?L"继续":L"暂停");
    EnableWindow(pauseButton,focus.active());
    EnableWindow(GetDlgItem(focusWindow,C_STOP),focus.active());
    std::wstring focusTitle=L"专注 "+std::to_wstring(focusMinutes)+L" 分钟",breakTitle=L"休息 "+std::to_wstring(breakMinutes)+L" 分钟";
    SetWindowTextW(GetDlgItem(focusWindow,C_CUSTOMFOCUS),focusTitle.c_str());SetWindowTextW(GetDlgItem(focusWindow,C_CUSTOMBREAK),breakTitle.c_str());
}
static void showFocus();
static void command(int id) {
    auto now=GetTickCount64();
    switch(id) {
    case C_STRETCH:case C_YAWN:case C_HAIR:case C_LOOK:
    case C_SWING:case C_THINK:case C_CLAP:case C_HOP: {
        Pose selected=static_cast<Pose>(int(Pose::Stretch)+id-C_STRETCH);
        pausedPose=false;setPose(selected,yuuki::actionDuration(selected));nextAction=now+8000;break;
    }
    case C_SHOW: if(visible){ visible=false; ShowWindow(petWindow,SW_HIDE); } else showPet(); break;
    case C_WANDER: wandering=!wandering; pausedPose=false; setPose(Pose::Idle); nextAction=now+4000; nextWalk=now+roll(20000,40000);anchorX=appX;break;
    case C_AUTOPOSE: automaticPoses=!automaticPoses;pausedPose=false;setPose(Pose::Idle);nextAction=now+3500;break;
    case C_READ:
        if(automaticPoses){pausedPose=false;setPose(Pose::Read,8000);}
        else{pausedPose=!(pausedPose&&pose==Pose::Read);setPose(pausedPose?Pose::Read:Pose::Idle);}
        nextAction=now+4000;break;
    case C_SLEEP:
        if(automaticPoses){pausedPose=false;setPose(Pose::Sleep,7000);}
        else{pausedPose=!(pausedPose&&pose==Pose::Sleep);setPose(pausedPose?Pose::Sleep:Pose::Idle);}
        nextAction=now+4000;break;
    case C_QUIET: quiet=!quiet; if(quiet) messageUntil=0; break;
    case C_REMIND: reminders=!reminders; nextReminder=now+reminderInterval(); break;
    case C_SMALL: case C_MEDIUM: case C_LARGE: {
        int oldH=height(),oldW=width(); petSize=id==C_SMALL?160:id==C_MEDIUM?220:280;
        appY+=oldH-height(); appX+=(oldW-width())/2; clampPosition();anchorX=appX;break;
    }
    case C_FOCUS: showFocus(); break;
    case C_SETTINGS: showSettings();break;
    case C_AUTOSTART:
        if(!setAutoStart(!autoStartEnabled()))MessageBoxW(nullptr,L"开机启动设置未能保存，请稍后重试。",L"由岐 · 设置",MB_OK|MB_ICONINFORMATION);
        break;
    case C_BIN: ShellExecuteW(nullptr,L"open",L"explorer.exe",L"shell:RecycleBinFolder",nullptr,SW_SHOWNORMAL);break;
    case C_AFFECTION: {
        std::wstring text=L"好感度："+std::to_wstring(affection.value)+L" / 100\n当前阶段："+affectionStage()+L"\n\n摸头 +1（间隔至少 30 秒），完成一次专注 +2。\n每天最多增加 10 点，离线不扣分。";
        MessageBoxW(nullptr,text.c_str(),L"由岐 · 好感度",MB_OK|MB_ICONINFORMATION);break;
    }
    case C_CUSTOMFOCUS:startFocus(focusMinutes);break;
    case C_CUSTOMBREAK:startFocus(breakMinutes,true);break;
    case C_25: startFocus(25); break;
    case C_50: startFocus(50); break;
    case C_BREAK: startFocus(5,true); break;
    case C_PAUSE: focus.togglePause(now); updateTip(); break;
    case C_STOP: focus.stop(); setPose(Pose::Idle); pausedPose=false; nextAction=now+6000; updateTip(); say(L"计时结束，按自己的节奏来。"); break;
    case C_HOME: { RECT r=workArea(true); appX=r.right-width()-60; appY=r.bottom-height();anchorX=appX;showPet();break; }
    case C_HELP: MessageBoxW(focusWindow?focusWindow:nullptr,
        L"由岐桌宠 0.1.2\n\n拖动：换位置或屏幕，拖到哪里就停在哪里\n自动姿势：读书、瞌睡、伸懒腰、揉眼、整理头发、张望、晃脚、托腮等\n偶尔在放置位置附近散步，不会吸回底部\n点击头部：摸头反应与好感度\n点击身体：打招呼；互动后偶尔拍手或小跳\n右键 → 动作：可逐个播放八个新动作；双击：读书\n右键：自动姿势、散步、大小、番茄钟和设置\n\n把本地普通文件拖到她身上，确认后送入回收站。\n文件夹、网络文件和系统文件暂不接受。\n右键 → 打开回收站，可以找回文件。\n\n设置和好感度保存在本机；退出会结束当前计时。\n移动程序文件前，请先关闭开机启动。",
        L"由岐桌宠 · 使用说明",MB_OK|MB_ICONINFORMATION); break;
    case C_EXIT: DestroyWindow(petWindow); return;
    }
    saveSettings(); focusUi(); render();
}
static void menu(POINT point) {
    menuOpen=true;
    HMENU m=CreatePopupMenu();
    auto item=[m](UINT id,const wchar_t* text,bool check=false){AppendMenuW(m,MF_STRING|(check?MF_CHECKED:0),id,text);};
    item(C_SHOW,visible?L"隐藏桌宠":L"显示桌宠"); item(C_HOME,L"回到屏幕右下角");
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    item(C_AUTOPOSE,L"自动变换姿势",automaticPoses);
    item(C_WANDER,L"在附近偶尔散步",wandering); item(C_READ,L"安静读书",pose==Pose::Read);
    item(C_SLEEP,L"打个瞌睡",pausedPose&&pose==Pose::Sleep);
    HMENU actions=CreatePopupMenu();
    const wchar_t* actionNames[]={L"伸懒腰",L"揉眼打哈欠",L"整理头发",L"左右张望",L"坐着晃脚",L"托腮发呆",L"轻轻拍手",L"原地小跳"};
    for(int i=0;i<8;++i)AppendMenuW(actions,MF_STRING,C_STRETCH+i,actionNames[i]);
    AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(actions),L"动作");
    HMENU sizes=CreatePopupMenu();
    AppendMenuW(sizes,MF_STRING|(petSize==160?MF_CHECKED:0),C_SMALL,L"小 · 160 px");
    AppendMenuW(sizes,MF_STRING|(petSize==220?MF_CHECKED:0),C_MEDIUM,L"中 · 220 px");
    AppendMenuW(sizes,MF_STRING|(petSize==280?MF_CHECKED:0),C_LARGE,L"大 · 280 px");
    AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(sizes),L"调整大小");
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    item(C_FOCUS,L"番茄钟…");
    std::wstring reminderText=L"每 "+std::to_wstring(reminderMinutes)+L" 分钟休息提醒";item(C_REMIND,reminderText.c_str(),reminders);
    item(C_SETTINGS,L"时间与启动设置…");item(C_AUTOSTART,L"开机启动",autoStartEnabled());
    std::wstring affectionText=L"好感度 "+std::to_wstring(affection.value)+L" · "+affectionStage();item(C_AFFECTION,affectionText.c_str());
    item(C_BIN,L"打开回收站");
    item(C_QUIET,L"安静模式",quiet);
    AppendMenuW(m,MF_SEPARATOR,0,nullptr); item(C_HELP,L"使用说明"); item(C_EXIT,L"退出");
    SetForegroundWindow(petWindow);
    int selected=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,petWindow,nullptr);
    DestroyMenu(m); PostMessageW(petWindow,WM_NULL,0,0); menuOpen=false;
    if(selected) command(selected);
}
static void tick() {
    auto now=GetTickCount64(); double dt=std::min(0.12,double(now-lastTick)/1000.0); lastTick=now;
    if(focus.tick(now)) {
        pausedPose=false;
        if(!focusIsBreak){affection.add(2,localDay());saveSettings();}
        notify(focusIsBreak?L"休息结束":L"专注完成",focusIsBreak?L"按自己的节奏，准备下一段专注吧。":L"这一段完成了。站起来走走、喝口水吧。");
        if(!quiet) say(focusIsBreak?L"回来啦，准备好就继续。":L"做得不错，休息一下吧。",7000);
        setPose(Pose::Greet,5000); nextAction=now+8000;
        nextReminder=now+reminderInterval(); updateTip();
    }
    if(reminders && now>=nextReminder) {
        nextReminder=now+reminderInterval();
        if(!focus.active()) { notify(L"由岐的休息提醒",L"坐了一会儿，喝点水、活动一下肩膀吧。"); if(!quiet) say(L"起来走走，喝口水吧。",6500); }
    }
    if(!dragging && !mouseDown && !menuOpen && !pausedPose && !modalBusy && !dragHover) {
        if(poseUntil && now>=poseUntil) {
            bool walked=pose==Pose::Walk;
            Pose response=followup;
            if(response!=Pose::Idle&&!focus.active())setPose(response,yuuki::actionDuration(response));
            else setPose(focus.active()&&!focusIsBreak?Pose::Read:Pose::Idle);
            nextAction=now+roll(3500,6000);
            if(walked)saveSettings();
        }
        if(automaticPoses&&!focus.active()&&now>=nextAction&&!poseUntil) {
            Pose choice=dailyActions.next(randomEngine);
            setPose(choice,yuuki::actionRow(choice)>=0?yuuki::actionDuration(choice):choice==Pose::Greet?2800:roll(4000,6500));
            nextAction=now+15000;
        }
        if(wandering&&!focus.active()&&now>=nextWalk&&pose==Pose::Idle){
            nextWalk=now+roll(20000,40000);direction=roll(0,1)?1:-1;
            setPose(Pose::Walk,roll(3000,5000));
        }
        if(pose==Pose::Walk) {
            movementRemainder+=direction*20*dt;
            int step=int(movementRemainder); movementRemainder-=step; appX+=step;
            RECT r=workArea();
            int center=yuuki::clampAxis(anchorX,width(),r.left,r.right);
            int low=std::max(int(r.left),center-100),high=std::min(int(r.right)-width(),center+100);
            high=std::max(low,high);
            if(appX<low||appX>high){direction=-direction;appX=std::clamp(appX,low,high);movementRemainder=0;}
        }
    }
    if(now-lastTipUpdate>=1000) {
        lastTipUpdate=now;
        int priorDay=affection.day;affection.refresh(localDay());if(priorDay!=affection.day)saveSettings();
        updateTip();focusUi();
    }
    render();
}
static LRESULT CALLBACK FocusProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        focusLabel=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,15,18,330,62,hwnd,nullptr,appInstance,nullptr);
        auto button=[&](const wchar_t* text,int id,int x,int y,int w){
            HWND b=CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,x,y,w,32,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),appInstance,nullptr);
            SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return b;
        };
        button(L"开始专注",C_CUSTOMFOCUS,15,94,104); button(L"开始休息",C_CUSTOMBREAK,128,94,104); button(L"设置时长",C_SETTINGS,241,94,104);
        pauseButton=button(L"暂停",C_PAUSE,68,144,104); button(L"结束计时",C_STOP,188,144,104);
        HWND note=CreateWindowW(L"STATIC",L"计时持续运行，关闭此面板不会结束计时。",WS_CHILD|WS_VISIBLE|SS_CENTER,15,196,330,24,hwnd,nullptr,appInstance,nullptr);
        SendMessageW(focusLabel,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); SendMessageW(note,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        return 0;
    }
    case WM_COMMAND: command(LOWORD(wp)); return 0;
    case WM_CLOSE: ShowWindow(hwnd,SW_HIDE); return 0;
    case WM_DESTROY: focusWindow=nullptr; return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void showFocus() {
    if(!focusWindow) {
        RECT r{0,0,360,235}; AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,WS_EX_TOOLWINDOW);
        RECT work=workArea();
        focusWindow=CreateWindowExW(WS_EX_TOOLWINDOW,L"YuukiFocusWindow",L"由岐 · 番茄钟",WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            work.left+(work.right-work.left-(r.right-r.left))/2,work.top+(work.bottom-work.top-(r.bottom-r.top))/2,
            r.right-r.left,r.bottom-r.top,nullptr,nullptr,appInstance,nullptr);
    }
    focusUi(); ShowWindow(focusWindow,SW_RESTORE); SetForegroundWindow(focusWindow);
}
static LRESULT CALLBACK SettingsProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto add=[&](DWORD ex,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
            HWND child=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),appInstance,nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;
        };
        add(0,L"STATIC",L"专注时间（1–180 分钟）",0,20,22,230,24);
        focusEdit=add(WS_EX_CLIENTEDGE,L"EDIT",L"25",ES_NUMBER|WS_TABSTOP,270,18,75,27,201);
        add(0,L"STATIC",L"休息时间（1–60 分钟）",0,20,64,230,24);
        breakEdit=add(WS_EX_CLIENTEDGE,L"EDIT",L"5",ES_NUMBER|WS_TABSTOP,270,60,75,27,202);
        add(0,L"STATIC",L"提醒间隔（5–180 分钟）",0,20,106,230,24);
        reminderEdit=add(WS_EX_CLIENTEDGE,L"EDIT",L"45",ES_NUMBER|WS_TABSTOP,270,102,75,27,203);
        reminderCheck=add(0,L"BUTTON",L"启用休息提醒（专注中不重复提醒）",BS_AUTOCHECKBOX|WS_TABSTOP,20,146,325,26,204);
        startupCheck=add(0,L"BUTTON",L"登录 Windows 后启动由岐",BS_AUTOCHECKBOX|WS_TABSTOP,20,184,325,26,205);
        affectionLabel=add(0,L"STATIC",L"",0,20,226,325,44);
        add(0,L"STATIC",L"新时长用于下次计时；好感度离线不扣分。",0,20,280,325,26);
        add(0,L"BUTTON",L"保存设置",BS_DEFPUSHBUTTON|WS_TABSTOP,225,323,120,34,C_APPLY);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==C_APPLY){
            BOOL a,b,c;UINT f=GetDlgItemInt(hwnd,201,&a,FALSE),r=GetDlgItemInt(hwnd,202,&b,FALSE),n=GetDlgItemInt(hwnd,203,&c,FALSE);
            if(!a||!b||!c||f<1||f>180||r<1||r>60||n<5||n>180){
                MessageBoxW(hwnd,L"请按标注范围填写整分钟数。",L"由岐 · 时间设置",MB_OK|MB_ICONINFORMATION);return 0;
            }
            bool startup=SendMessageW(startupCheck,BM_GETCHECK,0,0)==BST_CHECKED;
            if(startup!=autoStartEnabled()&&!setAutoStart(startup)){
                MessageBoxW(hwnd,L"开机启动设置未能保存，请稍后重试。",L"由岐 · 设置",MB_OK|MB_ICONINFORMATION);return 0;
            }
            focusMinutes=int(f);breakMinutes=int(r);reminderMinutes=int(n);reminders=SendMessageW(reminderCheck,BM_GETCHECK,0,0)==BST_CHECKED;
            nextReminder=GetTickCount64()+reminderInterval();saveSettings();focusUi();ShowWindow(hwnd,SW_HIDE);say(L"设置记住啦。");
        }
        return 0;
    case WM_CLOSE:ShowWindow(hwnd,SW_HIDE);return 0;
    case WM_DESTROY:settingsWindow=nullptr;return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void showSettings(){
    if(!settingsWindow){
        RECT rect{0,0,365,380};AdjustWindowRectEx(&rect,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_TOOLWINDOW);
        RECT work=workArea();int w=rect.right-rect.left,h=rect.bottom-rect.top;
        settingsWindow=CreateWindowExW(WS_EX_TOOLWINDOW,L"YuukiSettingsWindow",L"由岐 · 设置",WS_CAPTION|WS_SYSMENU,
            work.left+(work.right-work.left-w)/2,work.top+(work.bottom-work.top-h)/2,w,h,nullptr,nullptr,appInstance,nullptr);
    }
    if(!settingsWindow)return;
    SetWindowTextW(focusEdit,std::to_wstring(focusMinutes).c_str());SetWindowTextW(breakEdit,std::to_wstring(breakMinutes).c_str());SetWindowTextW(reminderEdit,std::to_wstring(reminderMinutes).c_str());
    SendMessageW(reminderCheck,BM_SETCHECK,reminders?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(startupCheck,BM_SETCHECK,autoStartEnabled()?BST_CHECKED:BST_UNCHECKED,0);
    std::wstring text=L"好感度："+std::to_wstring(affection.value)+L" / 100 · "+affectionStage()+L"\r\n今天已增加 "+std::to_wstring(affection.earned)+L" / 10 点";
    SetWindowTextW(affectionLabel,text.c_str());ShowWindow(settingsWindow,SW_RESTORE);SetForegroundWindow(settingsWindow);
}
static void fileDrop(const std::vector<std::wstring>& files){
    if(files.empty()||modalBusy)return;
    modalBusy=true;dragHover=false;setPose(Pose::Anticipate);render();
    for(const auto& file:files)if(!yuuki::ordinaryLocalFile(file)){
        MessageBoxW(nullptr,L"这次拖入的内容包含无法回收的项目。\n\n目前支持本地普通文件；文件夹、网络位置、系统文件和链接暂不接受。\n本次没有移动任何文件。",L"由岐 · 文件保留原处",MB_OK|MB_ICONINFORMATION);
        modalBusy=false;setPose(Pose::Idle);return;
    }
    bool confirmed=yuuki::confirmFiles(petWindow,files);
    auto result=yuuki::recycleConfirmed(files,confirmed,yuuki::recycleOne);
    modalBusy=false;pausedPose=false;nextAction=GetTickCount64()+4000;nextWalk=GetTickCount64()+roll(20000,40000);
    if(result.canceled){setPose(focus.active()&&!focusIsBreak?Pose::Read:Pose::Idle);render();return;}
    if(!result.succeeded.empty()){
        setPose(Pose::Eat,2200);
        std::wstring text=L"吃掉啦！已回收 "+std::to_wstring(result.succeeded.size())+L" 个文件。";say(text.c_str(),6500);
    }else setPose(Pose::Idle);
    if(!result.failed.empty()){
        std::wstring text=L"已送入回收站："+std::to_wstring(result.succeeded.size())+L" 个。\n以下文件未能确认回收，请检查原位置和回收站：\n\n";
        for(size_t i=0;i<std::min<size_t>(result.failed.size(),12);++i)text+=result.failed[i]+L"\n";
        if(result.failed.size()>12)text+=L"……\n";
        text+=L"\n可能正在被使用，或所在位置无法回收。";
        MessageBoxW(nullptr,text.c_str(),L"由岐 · 回收结果",MB_OK|MB_ICONINFORMATION);
    }
    render();
}
class PetDropTarget final:public IDropTarget{
    LONG refs=1;bool acceptable=false;
    bool hit(POINTL point){return visible&&!modalBusy&&point.x>=appX&&point.x<appX+width()&&point.y>=appY+BUBBLE_H&&point.y<appY+height();}
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
        if(!out)return E_POINTER;
        *out=nullptr;
        if(iid==IID_IUnknown||iid==IID_IDropTarget){*out=static_cast<IDropTarget*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{ULONG left=ULONG(InterlockedDecrement(&refs));if(!left)delete this;return left;}
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* object,DWORD,POINTL point,DWORD* effect)override{
        FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};acceptable=object&&SUCCEEDED(object->QueryGetData(&format));
        dragHover=acceptable&&hit(point);if(dragHover){setPose(Pose::Anticipate);say(L"给我的吗？放下来先确认一下。");render();}
        *effect=dragHover&&(*effect&DROPEFFECT_MOVE)?DROPEFFECT_MOVE:DROPEFFECT_NONE;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL point,DWORD* effect)override{
        dragHover=acceptable&&hit(point);*effect=dragHover&&(*effect&DROPEFFECT_MOVE)?DROPEFFECT_MOVE:DROPEFFECT_NONE;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave()override{acceptable=false;dragHover=false;setPose(focus.active()&&!focusIsBreak?Pose::Read:Pose::Idle);render();return S_OK;}
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* object,DWORD,POINTL point,DWORD* effect)override{
        bool allowed=acceptable&&hit(point)&&(*effect&DROPEFFECT_MOVE);*effect=DROPEFFECT_NONE;acceptable=false;dragHover=false;
        if(!allowed)return S_OK;
        FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium{};
        if(FAILED(object->GetData(&format,&medium)))return S_OK;
        HDROP drop=reinterpret_cast<HDROP>(medium.hGlobal);UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);std::vector<std::wstring> paths;
        if(count<=100){
            for(UINT i=0;i<count;++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::vector<wchar_t> name(size_t(len)+1);if(DragQueryFileW(drop,i,name.data(),len+1))paths.emplace_back(name.data());}
        }
        ReleaseStgMedium(&medium);
        if(count>100){MessageBoxW(nullptr,L"每次最多吃 100 个文件，请分批拖入。",L"由岐 · 吃文件",MB_OK|MB_ICONINFORMATION);return S_OK;}
        fileDrop(paths);
        // The target already performed the recycle. Never ask the drag source to delete originals.
        *effect=DROPEFFECT_NONE;return S_OK;
    }
};
static LRESULT CALLBACK PetProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(taskbarCreated && msg==taskbarCreated) { addTray(); return 0; }
    switch(msg) {
    case WM_COMMAND:command(LOWORD(wp));return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: {
        int x=GET_X_LPARAM(lp)-appX,y=GET_Y_LPARAM(lp)-appY;
        if(x<0||y<0||x>=width()||y>=height()) return HTTRANSPARENT;
        size_t i=size_t(y)*width()+x;
        return i<renderedAlpha.size() && renderedAlpha[i]>20?HTCLIENT:HTTRANSPARENT;
    }
    case WM_LBUTTONDOWN:
        mouseDown=true; dragging=false; GetCursorPos(&downMouse); downWindow={appX,appY}; SetCapture(hwnd); return 0;
    case WM_MOUSEMOVE:
        if(mouseDown) {
            POINT point; GetCursorPos(&point);
            if(std::abs(point.x-downMouse.x)+std::abs(point.y-downMouse.y)>6) {
                if(!dragging) { dragging=true; setPose(Pose::Grab); }
                appX=downWindow.x+point.x-downMouse.x; appY=downWindow.y+point.y-downMouse.y; render();
            }
        } return 0;
    case WM_LBUTTONUP:
        if(mouseDown) {
            bool moved=dragging; mouseDown=dragging=false; ReleaseCapture();
            clampPosition(); pausedPose=false;
            if(moved) { anchorX=appX;movementRemainder=0;setPose(Pose::Idle);nextAction=GetTickCount64()+3500;nextWalk=GetTickCount64()+roll(20000,40000);saveSettings(); }
            else {
                bool head=downMouse.y-appY-BUBBLE_H<int(petSize*0.48);
                setPose(head?Pose::Pat:Pose::Greet,head?1500:3000);
                if(!focus.active()&&roll(0,2)==0){
                    followup=lastReaction==Pose::Clap?Pose::Hop:lastReaction==Pose::Hop?Pose::Clap:roll(0,1)?Pose::Clap:Pose::Hop;
                    lastReaction=followup;
                }
                if(head){affection.pat(GetTickCount64(),localDay());saveSettings();}
                static const wchar_t* replies[]={L"嗯？我在呢。",L"好啦，去做你想做的事吧。",L"今天也慢慢来。",L"看会儿书？",L"被你发现我在偷懒啦。"};
                if(head)say(affection.value<40?L"嗯？轻一点嘛。":affection.value<70?L"又来摸头啦……好吧。":L"是你呀。那就再待一会儿。");
                else say(replies[roll(0,4)]);
                nextAction=GetTickCount64()+8000;
            }
            render();
        } return 0;
    case WM_CAPTURECHANGED:
        if(mouseDown) { mouseDown=dragging=false;clampPosition();anchorX=appX;setPose(Pose::Idle);nextAction=GetTickCount64()+3500;saveSettings(); }
        return 0;
    case WM_LBUTTONDBLCLK: mouseDown=dragging=false; ReleaseCapture(); command(C_READ); return 0;
    case WM_RBUTTONUP: { POINT point; GetCursorPos(&point); menu(point); return 0; }
    case WM_CONTEXTMENU: { POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; if(point.x==-1) GetCursorPos(&point); menu(point); return 0; }
    case TRAY_MESSAGE: {
        UINT event=LOWORD(lp);
        if(event==WM_CONTEXTMENU || event==WM_RBUTTONUP) { POINT point; GetCursorPos(&point); menu(point); }
        else if(event==WM_LBUTTONDBLCLK) command(C_SHOW);
        else if(event==NIN_SELECT || event==NIN_KEYSELECT) { if(!visible) showPet(); else say(L"我在这里。右键可以打开菜单。"); }
        return 0;
    }
    case RESTORE_PET: showPet(); say(L"我已经在这里啦。"); return 0;
    case WM_TIMER: tick(); return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: clampPosition(); render(); return 0;
    case WM_DPICHANGED: clampPosition(); render(); return 0;
    case WM_QUERYENDSESSION: saveSettings(); return TRUE;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if(oleReady)RevokeDragDrop(hwnd);
        saveSettings(); KillTimer(hwnd,1); tray.uFlags=0; Shell_NotifyIconW(NIM_DELETE,&tray);
        if(focusWindow) DestroyWindow(focusWindow);
        if(settingsWindow)DestroyWindow(settingsWindow);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static bool loadAtlas() {
    HRSRC resource=FindResourceW(appInstance,MAKEINTRESOURCEW(101),RT_RCDATA);
    if(!resource) return false;
    DWORD size=SizeofResource(appInstance,resource); HGLOBAL loaded=LoadResource(appInstance,resource);
    const void* bytes=LockResource(loaded); if(!bytes || !size) return false;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size); if(!memory) return false;
    void* target=GlobalLock(memory); if(!target) {GlobalFree(memory); return false;}
    CopyMemory(target,bytes,size); GlobalUnlock(memory);
    if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&atlasStream))) { GlobalFree(memory); return false; }
    atlas.reset(Bitmap::FromStream(atlasStream,FALSE));
    return atlas && atlas->GetLastStatus()==Ok && atlas->GetWidth()==CELL_W*4 && atlas->GetHeight()==CELL_H*13;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    appInstance=instance;
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\YuukiDesktopPet_01");
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        if(HWND existing=FindWindowW(PET_CLASS,nullptr)) PostMessageW(existing,RESTORE_PET,0,0);
        if(singleton) CloseHandle(singleton);
        return 0;
    }
    SetProcessDPIAware();
    struct OleScope{HRESULT status=OleInitialize(nullptr);~OleScope(){if(SUCCEEDED(status))OleUninitialize();}} ole;
    if(FAILED(ole.status)){MessageBoxW(nullptr,L"文件拖放初始化失败，请重启程序。",L"由岐桌宠",MB_OK|MB_ICONINFORMATION);if(singleton)CloseHandle(singleton);return 5;}
    oleReady=true;
    GdiplusStartupInput input; ULONG_PTR graphicsToken;
    if(GdiplusStartup(&graphicsToken,&input,nullptr)!=Ok) {if(singleton)CloseHandle(singleton); return 1;}
    if(!loadAtlas()) {
        MessageBoxW(nullptr,L"角色资源加载失败，请重新解压完整下载包。",L"由岐桌宠",MB_OK|MB_ICONERROR);
        atlas.reset(); if(atlasStream)atlasStream->Release(); GdiplusShutdown(graphicsToken); if(singleton)CloseHandle(singleton); return 2;
    }
    petIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    readSettings();
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.style=CS_DBLCLKS; wc.lpfnWndProc=PetProc;
    wc.hInstance=instance; wc.hIcon=petIcon; wc.hCursor=LoadCursorW(nullptr,IDC_HAND); wc.lpszClassName=PET_CLASS;
    if(!RegisterClassExW(&wc)) { atlas.reset(); atlasStream->Release(); GdiplusShutdown(graphicsToken); CloseHandle(singleton); return 3; }
    WNDCLASSEXW fc{}; fc.cbSize=sizeof(fc); fc.lpfnWndProc=FocusProc; fc.hInstance=instance; fc.hIcon=petIcon;
    fc.hCursor=LoadCursorW(nullptr,IDC_ARROW); fc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); fc.lpszClassName=L"YuukiFocusWindow";
    RegisterClassExW(&fc);
    WNDCLASSEXW sc=fc;sc.lpszClassName=L"YuukiSettingsWindow";sc.lpfnWndProc=SettingsProc;RegisterClassExW(&sc);
    petWindow=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,PET_CLASS,L"由岐桌宠",WS_POPUP,appX,appY,width(),height(),nullptr,nullptr,instance,nullptr);
    if(!petWindow) { atlas.reset(); atlasStream->Release(); GdiplusShutdown(graphicsToken); CloseHandle(singleton); return 4; }
    auto* dropTarget=new PetDropTarget();HRESULT dropStatus=RegisterDragDrop(petWindow,dropTarget);dropTarget->Release();
    if(FAILED(dropStatus)){DestroyWindow(petWindow);MessageBoxW(nullptr,L"文件拖放注册失败，请重启程序。",L"由岐桌宠",MB_OK|MB_ICONINFORMATION);atlas.reset();atlasStream->Release();GdiplusShutdown(graphicsToken);if(singleton)CloseHandle(singleton);return 6;}
    poseSince=lastTick=GetTickCount64();nextAction=lastTick+3500;nextWalk=lastTick+roll(20000,40000);nextReminder=lastTick+reminderInterval();
    addTray(); showPet(); if(!quiet) say(L"你好呀。右键我可以打开菜单。",7000);
    SetTimer(petWindow,1,60,nullptr);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        if(settingsWindow&&IsWindowVisible(settingsWindow)&&IsDialogMessageW(settingsWindow,&msg))continue;
        if(!focusWindow || !IsWindowVisible(focusWindow) || !IsDialogMessageW(focusWindow,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    atlas.reset(); atlasStream->Release(); GdiplusShutdown(graphicsToken); if(singleton)CloseHandle(singleton);
    return 0;
}
