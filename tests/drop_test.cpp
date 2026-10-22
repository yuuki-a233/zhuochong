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
#include <thread>
#include <atomic>
#include "../src/commands.hpp"

class FileData final:public IDataObject{
    LONG refs=1;std::wstring path;
public:
    explicit FileData(std::wstring p):path(std::move(p)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IDataObject){*out=static_cast<IDataObject*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{auto n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f)override{return f&&f->cfFormat==CF_HDROP&&(f->tymed&TYMED_HGLOBAL)?S_OK:DV_E_FORMATETC;}
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f,STGMEDIUM* m)override{
        if(FAILED(QueryGetData(f)))return DV_E_FORMATETC;
        size_t bytes=sizeof(DROPFILES)+(path.size()+2)*sizeof(wchar_t);
        HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!memory)return E_OUTOFMEMORY;
        auto* drop=static_cast<DROPFILES*>(GlobalLock(memory));drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;
        CopyMemory(reinterpret_cast<BYTE*>(drop)+sizeof(DROPFILES),path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);
        m->tymed=TYMED_HGLOBAL;m->hGlobal=memory;m->pUnkForRelease=nullptr;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*,STGMEDIUM*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,FORMATETC* out)override{if(out)out->ptd=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*,STGMEDIUM*,BOOL)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD,IEnumFORMATETC**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*,DWORD,IAdviseSink*,DWORD*)override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD)override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**)override{return OLE_E_ADVISENOTSUPPORTED;}
};
class DropSource final:public IDropSource{
    LONG refs=1;ULONGLONG start=GetTickCount64();
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IDropSource){*out=static_cast<IDropSource*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{auto n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape,DWORD)override{if(escape)return DRAGDROP_S_CANCEL;return GetTickCount64()-start>600?DRAGDROP_S_DROP:S_OK;}
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD)override{return DRAGDROP_S_USEDEFAULTCURSORS;}
};
int wmain(int argc,wchar_t**){
    bool accept=argc>1;
    assert(SUCCEEDED(OleInitialize(nullptr)));
    HWND pet=FindWindowW(L"YuukiDesktopPetWindow",nullptr);assert(pet);
    SendMessageW(pet,WM_COMMAND,C_HOME,0);SendMessageW(pet,WM_COMMAND,C_WANDER,0);
    wchar_t temp[MAX_PATH],file[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"ykd",0,file);
    RECT r;GetWindowRect(pet,&r);SetCursorPos((r.left+r.right)/2,r.top+120);Sleep(150);
    std::atomic<bool> confirmed{false};
    std::thread closer([&]{
        for(int i=0;i<100;++i){
            if(HWND dialog=FindWindowW(L"YuukiFileConfirmation",nullptr)){
                wchar_t shown[1024]{};SendMessageW(GetDlgItem(dialog,10),WM_GETTEXT,1024,reinterpret_cast<LPARAM>(shown));
                assert(std::wstring(shown).find(file)!=std::wstring::npos);
                confirmed=true;SendMessageW(dialog,WM_COMMAND,accept?IDYES:IDCANCEL,0);
                if(accept){
                    for(int j=0;j<80;++j){
                        if(HWND result=FindWindowW(nullptr,L"由岐 · 回收结果")){SendMessageW(result,WM_COMMAND,IDOK,0);return;}
                        Sleep(100);
                    }
                }
                return;
            }
            Sleep(100);
        }
    });
    auto* data=new FileData(file);auto* source=new DropSource();DWORD effect=0;
    HRESULT result=DoDragDrop(data,source,DROPEFFECT_MOVE,&effect);
    data->Release();source->Release();closer.join();
    assert(confirmed&&result==DRAGDROP_S_DROP&&effect==DROPEFFECT_NONE);
    if(!accept)assert(GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES);
    if(GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES)DeleteFileW(file);
    SendMessageW(pet,WM_COMMAND,C_WANDER,0);
    std::puts(accept?"Real OLE confirmation accepted; unavailable native recycle reports failure without deleting source.":"Real OLE file drag, full-path confirmation and cancellation retaining original passed.");
    OleUninitialize();return 0;
}
