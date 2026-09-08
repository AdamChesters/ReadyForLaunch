#pragma once
#include "engine.hpp"
#include <windows.h>
#include <objbase.h>
#include <memory>
#include <unordered_map>

namespace rfl {
std::wstring wide(const std::string& value);
std::string utf8(const std::wstring& value);
std::string systemError(DWORD code=GetLastError());
std::filesystem::path dataDirectory();
std::filesystem::path appDirectory();
std::string sha256(const std::filesystem::path& path);
std::string browseExe(HWND owner);
std::wstring quote(const std::wstring& argument);
struct AppChoice {
    std::string name, description, target, directory, probe, arguments, application;
    Source source=Source::Exe;
};
std::vector<AppChoice> runningApps();
std::vector<AppChoice> steamApps();
std::vector<AppChoice> installedApps();
bool normalizeLaunchTarget(Task& task);
std::filesystem::path targetFolder(const Task& task);
std::unordered_map<std::string,bool> observeApps(const std::vector<Task>& tasks);
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
