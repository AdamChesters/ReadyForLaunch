#pragma once
#include "windows.hpp"
#include "updates.hpp"
#include "images.hpp"
#include <future>
#include <unordered_set>

namespace rfl {
class Ui {
    friend struct UiTestAccess;
    HWND window_;std::filesystem::path directory_;
    Settings settings_;WindowsBackend backend_;Engine engine_{backend_};
    UpdateClient updates_;ID3D11Device* device_;Texture helpImage_;
    std::string message_,activeProfile_;
    std::future<std::vector<AppChoice>> discovery_;
    std::vector<AppChoice> choices_;
    int pickerTab_=0,loadingTab_=-1;bool pickerOpen_=false,editOpen_=false,profileOpen_=false,groupOpen_=false,settingsOpen_=false;
    std::string editGroup_,editTask_,groupId_;
    Task draft_;std::string search_,profileName_,groupName_;
    bool dirty_=false,preview_=false;
    std::string captureView_;
    std::vector<std::string> log_;
    std::future<std::unordered_map<std::string,bool>> observation_;
    std::unordered_map<std::string,bool> live_;
    std::unordered_map<std::string,std::string> seen_;
    std::unordered_set<std::string> warnedReused_;
    uint64_t lastObservation_=0;
    bool statusOpen_=false,helpOpen_=false,updateOpen_=false,installRequested_=false;
    std::string profileAction_,actionSource_,actionTarget_,actionName_;
    bool actionPending_=false;
    Profile& selected();
    void save();void scan(int tab);void appPicker();void taskEditor();void profileEditor();void groupEditor();
    void startEdit(const std::string& group,const std::string& task={});
    void groupGate(Profile& profile,Group& group);
    void rows(Profile& profile,Group& group,bool active);
    void moveTask(const std::string& id,const std::string& destination,int before=-1);
    void note(const std::string& text,bool alert=false);
    void profileContext(Profile& profile);void profileActionDialog();
    void helpWindow();void statusWindow();void updateControls();
    Profile* profileById(const std::string& id);
    void beginSession();
public:
    Ui(HWND window,ID3D11Device* device,std::filesystem::path data,bool preview=false,bool offline=false);
    void render();void tick();
    void captureView(std::string view){captureView_=std::move(view);}
    bool active(){return engine_.active();}
    void stop(){engine_.stop(GetTickCount64());note("Stop requested. Asking detected apps to close gracefully.");}
};
void applyTheme();
}
