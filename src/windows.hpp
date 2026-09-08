#pragma once
#include "engine.hpp"
#include <windows.h>
#include <objbase.h>
#include <memory>

namespace rfl {
std::wstring wide(const std::string& value);
std::string utf8(const std::wstring& value);
std::string systemError(DWORD code=GetLastError());
std::filesystem::path dataDirectory();
std::string browseExe(HWND owner);
std::wstring quote(const std::wstring& argument);
struct AppChoice {
    std::string name, description, target, directory, probe;
    Source source=Source::Exe;
};
std::vector<AppChoice> runningApps();
std::vector<AppChoice> steamApps();
std::vector<AppChoice> installedApps();
class WindowsBackend final:public Backend {
    struct Impl; std::unique_ptr<Impl> impl_;
public:
    WindowsBackend();~WindowsBackend()override;
    Launch start(const Task& task)override;
    Observation poll(int ticket)override;
    std::string close(int ticket,bool force)override;
    bool ownedAlive(int ticket)override;
};
}
