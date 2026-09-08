#include <windows.h>
#include <shellapi.h>
#include <string>
#include <filesystem>

namespace {
bool refuse=false;
LRESULT CALLBACK proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_CLOSE){if(!refuse)DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    auto command=std::wstring(GetCommandLineW());
    if(command.find(L"--processStart")!=std::wstring::npos) {
        wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);auto root=std::filesystem::path(module).parent_path();
        std::filesystem::path version;for(auto& entry:std::filesystem::directory_iterator(root))if(entry.is_directory()&&entry.path().filename().wstring().rfind(L"app-",0)==0&&entry.path().filename()>version.filename())version=entry.path();
        auto executable=version/L"Fixture.exe";std::wstring arguments=L"\""+executable.wstring()+L"\" --visible";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
        if(!CreateProcessW(executable.c_str(),arguments.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&info))return 7;
        CloseHandle(info.hThread);CloseHandle(info.hProcess);return 0;
    }
    if(command.find(L"--success")!=std::wstring::npos){Sleep(150);return 0;}
    if(command.find(L"--failure")!=std::wstring::npos)return 7;
    refuse=command.find(L"--refuse")!=std::wstring::npos;
    if(command.find(L"--child")!=std::wstring::npos) {
        wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);std::wstring cmd=L"\""+std::wstring(exe)+L"\" --refuse";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
        if(CreateProcessW(exe,cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&info)){CloseHandle(info.hThread);CloseHandle(info.hProcess);}
    }
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"ReadyForLaunch.TestFixture";RegisterClassW(&wc);
    DWORD extra=command.find(L"--tool")!=std::wstring::npos?WS_EX_TOOLWINDOW:0;
    auto window=CreateWindowExW(extra,wc.lpszClassName,L"ReadyForLaunch test fixture",WS_OVERLAPPEDWINDOW,-30000,-30000,320,180,nullptr,nullptr,instance,nullptr);
    if(!window)return 2;
    if(command.find(L"--visible")!=std::wstring::npos)ShowWindow(window,SW_SHOWNOACTIVATE);
    // Hidden test window still accepts graceful WM_CLOSE; never interrupts the desktop.
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
