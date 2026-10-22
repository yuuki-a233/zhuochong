#pragma once
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <string>
#include <vector>

namespace yuuki {
template<class T> struct ComPtr {
    T* value=nullptr;
    ~ComPtr(){if(value)value->Release();}
    ComPtr()=default;ComPtr(const ComPtr&)=delete;ComPtr& operator=(const ComPtr&)=delete;
    T* operator->() const{return value;}
    T** out(){return &value;}
};
inline bool ordinaryLocalFile(const std::wstring& path) {
    if(path.size()<4 || path[1]!=L':' || path[2]!=L'\\')return false;
    auto attr=GetFileAttributesW(path.c_str());
    if(attr==INVALID_FILE_ATTRIBUTES || attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_SYSTEM))return false;
    wchar_t root[]={path[0],L':',L'\\',0};
    auto type=GetDriveTypeW(root);
    return type==DRIVE_FIXED || type==DRIVE_REMOVABLE;
}
inline HRESULT recycleOne(const std::wstring& path) {
    if(!ordinaryLocalFile(path))return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    ComPtr<IShellItem> item,parent,bin,recycled;
    ComPtr<ITransferSource> source;
    HRESULT hr=SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(item.out()));
    if(SUCCEEDED(hr))hr=item->GetParent(parent.out());
    if(SUCCEEDED(hr))hr=parent->BindToHandler(nullptr,BHID_Transfer,IID_PPV_ARGS(source.out()));
    if(SUCCEEDED(hr))hr=SHGetKnownFolderItem(FOLDERID_RecycleBinFolder,KF_FLAG_DEFAULT,nullptr,IID_PPV_ARGS(bin.out()));
    // This API only recycles. Never substitute RemoveItem, DeleteFile, or a deletion operation.
    if(SUCCEEDED(hr))hr=source->RecycleItem(item.value,bin.value,TSF_NORMAL,recycled.out());
    if(FAILED(hr))return hr;
    if(!recycled.value || GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)return E_FAIL;
    SHChangeNotify(SHCNE_DELETE,SHCNF_PATHW,path.c_str(),nullptr);
    return S_OK;
}
struct RecycleResult {
    std::vector<std::wstring> succeeded,failed;
    bool canceled=false;
};
template<class Backend>
RecycleResult recycleConfirmed(const std::vector<std::wstring>& files,bool confirmed,Backend backend) {
    RecycleResult result;
    if(!confirmed){result.canceled=true;return result;}
    for(const auto& path:files){
        if(backend(path)==S_OK)result.succeeded.push_back(path);
        else result.failed.push_back(path);
    }
    return result;
}
}
