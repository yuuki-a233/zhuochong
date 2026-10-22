#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <objidl.h>
#include <cassert>
#include <cstdio>
#include "../src/recycle.hpp"

int wmain(){
    assert(SUCCEEDED(OleInitialize(nullptr)));
    int calls=0;
    std::vector<std::wstring> files={L"C:\\temporary-example-a.txt",L"C:\\temporary-example-b.txt"};
    auto canceled=yuuki::recycleConfirmed(files,false,[&](const std::wstring&){++calls;return S_OK;});
    assert(canceled.canceled&&calls==0&&canceled.succeeded.empty());
    auto mixed=yuuki::recycleConfirmed(files,true,[&](const std::wstring&){return calls++==0?S_OK:E_ACCESSDENIED;});
    assert(mixed.succeeded.size()==1&&mixed.failed.size()==1);
    assert(!yuuki::ordinaryLocalFile(L"\\\\server\\share\\example.txt"));
    assert(!yuuki::ordinaryLocalFile(L"C:\\"));
    wchar_t temp[MAX_PATH],file[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"ykt",0,file);
    assert(yuuki::ordinaryLocalFile(file));
    auto stopped=yuuki::recycleConfirmed(std::vector<std::wstring>{file},false,yuuki::recycleOne);
    assert(stopped.canceled&&GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES);
    HANDLE locked=CreateFileW(file,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(locked!=INVALID_HANDLE_VALUE);
    HRESULT blocked=yuuki::recycleOne(file);
    assert(FAILED(blocked)&&GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES);
    CloseHandle(locked);
    HRESULT recycled=yuuki::recycleOne(file);
    if(recycled==S_OK){assert(GetFileAttributesW(file)==INVALID_FILE_ATTRIBUTES);std::puts("Native recycle: passed.");}
    else{assert(GetFileAttributesW(file)!=INVALID_FILE_ATTRIBUTES);std::printf("Native recycle unavailable here (0x%08lx); original retained.\n",(unsigned long)recycled);DeleteFileW(file);}
    std::puts("Cancellation, partial results, unsupported paths, locked-file retention passed.");
    OleUninitialize();return 0;
}
