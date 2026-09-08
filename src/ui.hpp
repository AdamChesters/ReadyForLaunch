#pragma once
#include "windows.hpp"
#include <future>

namespace rfl {
class Ui {
    HWND window_;std::filesystem::path directory_;
    Settings settings_;WindowsBackend backend_;Engine engine_{backend_};
    std::string message_,activeProfile_;
    std::future<std::vector<AppChoice>> discovery_;
    std::vector<AppChoice> choices_;
    int pickerTab_=0,loadingTab_=-1;bool pickerOpen_=false,editOpen_=false,profileOpen_=false,groupOpen_=false,settingsOpen_=false;
    std::string editGroup_,editTask_,groupId_;
    Task draft_;std::string search_,profileName_,groupName_;
    bool dirty_=false,preview_=false;
    std::string captureView_;
    std::vector<std::string> log_;
    Profile& selected();
    void save();void scan(int tab);void appPicker();void taskEditor();void profileEditor();void groupEditor();
    void startEdit(const std::string& group,const std::string& task={});
    void groupGate(Profile& profile,Group& group);
    void rows(Profile& profile,Group& group,bool active);
    void moveTask(const std::string& id,const std::string& destination,int before=-1);
    void note(const std::string& text);
public:
    Ui(HWND window,std::filesystem::path data,bool preview=false);
    void render();void tick();
    void captureView(std::string view){captureView_=std::move(view);}
    bool active(){return engine_.active();}
    void stop(bool force){engine_.stop(force,GetTickCount64());}
};
void applyTheme();
}
