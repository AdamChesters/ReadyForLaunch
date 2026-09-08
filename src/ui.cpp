#include "ui.hpp"
#include <imgui.h>
#include <shellapi.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <functional>
#include <cmath>

namespace rfl {
namespace {
constexpr ImVec4 accent{.35f,.73f,.84f,1},muted{.55f,.59f,.62f,1},green{.38f,.83f,.60f,1},red{.94f,.46f,.48f,1};
bool input(const char* label,std::string& value,size_t capacity=32768) {
    std::vector<char> buffer(std::max(capacity,value.size()+256),0);std::copy(value.begin(),value.end(),buffer.begin());
    std::string id=label;if(id.rfind("##",0)!=0){ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);id="##"+id;}
    if(ImGui::InputText(id.c_str(),buffer.data(),buffer.size())){value=buffer.data();return true;}return false;
}
void secondary(const std::string& text){ImGui::PushStyleColor(ImGuiCol_Text,muted);ImGui::TextWrapped("%s",text.c_str());ImGui::PopStyleColor();}
void tooltip(const std::string& text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text.c_str());}
void right(float width){ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-width);}
bool button(const char* label,ImVec2 size,ImVec4 color) {ImGui::PushStyleColor(ImGuiCol_Button,color);bool pressed=ImGui::Button(label,size);ImGui::PopStyleColor();return pressed;}
bool seconds(const char* label,int& ms,int minimum=0) {float value=float(ms)/1000; if(ImGui::InputFloat(label,&value,1,5,"%.1f s")){if(std::isfinite(value))ms=int(std::clamp(double(value)*1000,double(minimum),3600000.0));return true;}return false;}
std::string groupName(const Profile& p,const std::string& id) {for(auto& g:p.groups)if(g.id==id)return g.name;return "Missing group";}
const char* readinessLabel(Readiness r) {static const char* values[]={"Process detected","Window responding","Successful completion","Confirm ready manually"};return values[int(r)];}
std::string timingLabel(const Task& t) {
    if(t.timing==Timing::Together)return "With previous";
    if(t.timing==Timing::Started)return "After previous starts";
    if(t.timing==Timing::Finished)return "After previous finishes";
    std::ostringstream text;text<<float(t.delayMs)/1000<<" s after previous";return text.str();
}
}
void applyTheme() {
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();s.WindowPadding={22,18};s.FramePadding={9,5};s.ItemSpacing={10,7};s.CellPadding={8,3};
    s.FrameRounding=5;s.ChildRounding=8;s.PopupRounding=8;s.WindowRounding=8;s.ScrollbarRounding=8;s.ChildBorderSize=1;
    auto& c=s.Colors;c[ImGuiCol_WindowBg]={.025f,.029f,.034f,1};c[ImGuiCol_ChildBg]={.042f,.052f,.06f,1};c[ImGuiCol_PopupBg]={.045f,.06f,.072f,1};
    c[ImGuiCol_Text]={.92f,.93f,.94f,1};c[ImGuiCol_TextDisabled]=muted;c[ImGuiCol_Border]={.15f,.20f,.22f,1};c[ImGuiCol_Separator]=c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg]={.08f,.10f,.12f,1};c[ImGuiCol_FrameBgHovered]={.12f,.19f,.22f,1};c[ImGuiCol_FrameBgActive]={.13f,.25f,.29f,1};
    c[ImGuiCol_Button]={.09f,.15f,.18f,1};c[ImGuiCol_ButtonHovered]={.14f,.28f,.33f,1};c[ImGuiCol_ButtonActive]={.18f,.36f,.42f,1};
    c[ImGuiCol_Header]={.10f,.19f,.23f,1};c[ImGuiCol_HeaderHovered]={.13f,.25f,.29f,1};c[ImGuiCol_HeaderActive]={.15f,.30f,.35f,1};
    c[ImGuiCol_CheckMark]=accent;c[ImGuiCol_SliderGrab]=accent;c[ImGuiCol_TabSelected]={.14f,.29f,.34f,1};
    c[ImGuiCol_TitleBg]={.05f,.08f,.10f,1};c[ImGuiCol_TitleBgActive]={.08f,.15f,.18f,1};c[ImGuiCol_Tab]={.055f,.075f,.09f,1};c[ImGuiCol_TabHovered]={.14f,.28f,.33f,1};c[ImGuiCol_ModalWindowDimBg]={0,0,0,.60f};
}
Ui::Ui(HWND window,std::filesystem::path data,bool preview):window_(window),directory_(std::move(data)),preview_(preview) {
    if(preview_) {
        settings_=defaults();auto& p=settings_.profiles.front();p.groups.clear();
        Group vr{newId(),"VR & haptics",{},{}},tools{newId(),"Flight tools",{},{}},sim{newId(),"Simulator",vr.id,{}};
        auto add=[](Group& g,const char* name,Source source,Timing timing){Task t;t.id=newId();t.name=name;t.source=source;t.target="preview";t.timing=timing;g.tasks.push_back(t);};
        add(vr,"VR2JB",Source::Exe,Timing::Started);vr.tasks.back().readiness=Readiness::Completion;
        add(vr,"SteamVR",Source::Steam,Timing::Finished);add(vr,"PSVR2SimShaker",Source::Exe,Timing::Started);
        add(tools,"VoiceAttack",Source::Steam,Timing::Started);add(tools,"OpenKneeboard",Source::Installed,Timing::Delay);add(tools,"SRS",Source::Exe,Timing::Together);
        add(sim,"DCS World",Source::Exe,Timing::Started);p.groups={vr,tools,sim};
    }else settings_=loadSettings(directory_/L"settings.json",message_);
}
Profile& Ui::selected(){for(auto& p:settings_.profiles)if(p.id==settings_.selected)return p;return settings_.profiles.front();}
void Ui::note(const std::string& text){message_=text;log_.push_back(text);if(log_.size()>100)log_.erase(log_.begin());}
void Ui::save(){if(preview_)return;try{saveSettings(directory_/L"settings.json",settings_);dirty_=false;}catch(const std::exception& e){note(e.what());}}
void Ui::tick(){engine_.tick(GetTickCount64());}
void Ui::scan(int tab) {
    if(discovery_.valid())return;
    choices_.clear();loadingTab_=tab;
    discovery_=std::async(std::launch::async,[tab]{HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);std::vector<AppChoice> result;
        try {result=tab==0?runningApps():tab==2?steamApps():installedApps();}catch(...){if(SUCCEEDED(hr))CoUninitialize();throw;}
        if(SUCCEEDED(hr))CoUninitialize();return result;
    });
}
void Ui::startEdit(const std::string& group,const std::string& task) {
    editGroup_=group;editTask_=task;draft_=Task{};draft_.id=newId();search_.clear();
    if(task.empty()){pickerOpen_=true;pickerTab_=0;scan(0);ImGui::OpenPopup("Add app");}
    else {for(auto& g:selected().groups)for(auto& t:g.tasks)if(t.id==task)draft_=t;editOpen_=true;ImGui::OpenPopup("App details");}
}
void Ui::moveTask(const std::string& id,const std::string& destination,int before) {
    auto& p=selected();Task moved;bool found=false;int oldPosition=-1;std::string origin;
    for(auto& g:p.groups)for(size_t i=0;i<g.tasks.size();++i)if(g.tasks[i].id==id){moved=g.tasks[i];oldPosition=int(i);origin=g.id;g.tasks.erase(g.tasks.begin()+i);found=true;break;}
    if(!found)return;
    for(auto& g:p.groups)if(g.id==destination) {
        if(origin==destination&&before>oldPosition)--before;
        if(before<0||before>int(g.tasks.size()))g.tasks.push_back(moved);else g.tasks.insert(g.tasks.begin()+before,moved);
    }dirty_=true;
}
void Ui::groupGate(Profile& profile,Group& group) {
    std::string label=group.afterGroup.empty()?"On GO":"After group: "+groupName(profile,group.afterGroup);
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##GroupStart",label.c_str())) {
        if(ImGui::Selectable("On GO",group.afterGroup.empty())){group.afterGroup.clear();dirty_=true;}
        ImGui::SeparatorText("After group...");
        for(auto& other:profile.groups)if(other.id!=group.id) {
            bool valid=canFollow(profile,group.id,other.id)&&std::any_of(other.tasks.begin(),other.tasks.end(),[](auto& t){return t.enabled;});
            ImGui::BeginDisabled(!valid);if(ImGui::Selectable(other.name.c_str(),group.afterGroup==other.id)){group.afterGroup=other.id;dirty_=true;}ImGui::EndDisabled();
            if(!valid)tooltip("This group is empty or would create a circular dependency.");
        }ImGui::EndCombo();
    }tooltip(label+". This rule stays with the group when its first task changes.");
}
void Ui::rows(Profile& p,Group& g,bool active) {
    std::function<void()> mutation;
    ImGuiTableFlags flags=ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH;
    if(ImGui::BeginTable("Apps",6,flags)) {
        ImGui::TableSetupColumn("Move",ImGuiTableColumnFlags_WidthFixed,18);
        ImGui::TableSetupColumn("Enable",ImGuiTableColumnFlags_WidthFixed,25);
        ImGui::TableSetupColumn("Application",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn("Timing",ImGuiTableColumnFlags_WidthFixed,252);
        ImGui::TableSetupColumn("State",ImGuiTableColumnFlags_WidthFixed,130);
        ImGui::TableSetupColumn("Edit",ImGuiTableColumnFlags_WidthFixed,30);
        bool first=true;
        for(size_t i=0;i<g.tasks.size();++i) {
            auto& t=g.tasks[i];ImGui::PushID(t.id.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();
            ImGui::BeginDisabled(active);ImGui::Selectable("::",false,0,{18,28});
            if(ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("RFL_TASK",t.id.c_str(),t.id.size()+1);ImGui::TextUnformatted(t.name.c_str());ImGui::EndDragDropSource();}
            if(ImGui::BeginDragDropTarget()){if(auto payload=ImGui::AcceptDragDropPayload("RFL_TASK")){auto id=std::string(static_cast<const char*>(payload->Data));auto dest=g.id;int index=int(i);mutation=[this,id,dest,index]{moveTask(id,dest,index);};}ImGui::EndDragDropTarget();}
            ImGui::TableNextColumn();if(ImGui::Checkbox("##Enabled",&t.enabled))dirty_=true;ImGui::EndDisabled();
            ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size>2?ImGui::GetIO().Fonts->Fonts[2]:nullptr,19);ImGui::TextUnformatted(t.name.c_str());ImGui::PopFont();
            tooltip(t.target);ImGui::SameLine();ImGui::PushFont(nullptr,12);ImGui::TextColored(muted," %s",sourceName(t.source));ImGui::PopFont();
            ImGui::TableNextColumn();ImGui::BeginDisabled(active||!t.enabled);
            if(first&&t.enabled)groupGate(p,g);
            else {
                auto label=timingLabel(t);ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##Timing",label.c_str())) {
                    const char* names[]={"With previous","After previous starts","Delay after previous","After previous finishes"};
                    for(int k=0;k<4;++k)if(ImGui::Selectable(names[k],int(t.timing)==k)){t.timing=Timing(k);dirty_=true;}
                    if(t.timing==Timing::Delay)dirty_|=seconds("Delay",t.delayMs);
                    ImGui::EndCombo();
                }
            }ImGui::EndDisabled();if(t.enabled)first=false;
            ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();const Node* state=activeProfile_==p.id?engine_.find(t.id):nullptr;
            if(state) {
                ImVec4 color=state->phase==Phase::Failed||state->phase==Phase::Attention?red:state->ready?green:muted;
                const char* text=state->phase==Phase::Running&&state->launch.reused?"Already running":phaseName(state->phase);
                ImGui::TextColored(color,"%s",text);tooltip(state->detail);
                if(state->phase==Phase::Launching&&t.readiness==Readiness::Manual){if(ImGui::SmallButton("Confirm ready"))engine_.confirm(t.id);}
                if(state->phase==Phase::Failed&&!engine_.stopping()){if(ImGui::SmallButton("Retry")){if(!engine_.retry(t.id))note("Close the existing failed app before retrying.");}}
            }else ImGui::TextColored(muted,"%s",t.enabled?"Idle":"Disabled");
            ImGui::TableNextColumn();ImGui::BeginDisabled(active);if(ImGui::SmallButton("..."))ImGui::OpenPopup("Actions");
            if(ImGui::BeginPopup("Actions")) {
                if(ImGui::MenuItem("Edit app")){auto gid=g.id,tid=t.id;mutation=[this,gid,tid]{startEdit(gid,tid);};}
                if(ImGui::MenuItem("Move up",nullptr,false,i>0)){auto id=t.id,dest=g.id;int pos=int(i)-1;mutation=[this,id,dest,pos]{moveTask(id,dest,pos);};}
                if(ImGui::MenuItem("Move down",nullptr,false,i+1<g.tasks.size())){auto id=t.id,dest=g.id;int pos=int(i)+2;mutation=[this,id,dest,pos]{moveTask(id,dest,pos);};}
                if(ImGui::BeginMenu("Move to group")){for(auto& other:p.groups)if(other.id!=g.id&&ImGui::MenuItem(other.name.c_str())){auto id=t.id,dest=other.id;mutation=[this,id,dest]{moveTask(id,dest);};}ImGui::EndMenu();}
                ImGui::Separator();if(ImGui::MenuItem("Remove app")){auto id=t.id;mutation=[this,id]{for(auto& group:selected().groups)std::erase_if(group.tasks,[&](auto& entry){return entry.id==id;});dirty_=true;};}
                ImGui::EndPopup();
            }ImGui::EndDisabled();ImGui::PopID();
        }ImGui::EndTable();
    }
    if(mutation)mutation();
}
void Ui::appPicker() {
    ImGui::SetNextWindowSize({760,575},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Add app",&pickerOpen_,ImGuiWindowFlags_NoSavedSettings))return;
    if(discovery_.valid()&&discovery_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        try {auto found=discovery_.get();if(loadingTab_==pickerTab_)choices_=std::move(found);}catch(const std::exception& e){note(e.what());}
        if(loadingTab_!=pickerTab_&&pickerTab_!=1)scan(pickerTab_);
    }
    if(ImGui::BeginTabBar("Sources")) {
        const char* tabs[]={"Running apps","Browse EXE","Steam","Installed apps"};
        for(int k=0;k<4;++k)if(ImGui::BeginTabItem(tabs[k])) {
            if(pickerTab_!=k){pickerTab_=k;choices_.clear();search_.clear();if(k!=1)scan(k);}
            ImGui::EndTabItem();
        }ImGui::EndTabBar();
    }
    bool choose=false;
    if(pickerTab_==1) {
        secondary("Choose the executable for an app you want to launch.");
        if(ImGui::Button("Browse for EXE...",{220,40})) {
            auto file=browseExe(window_);if(!file.empty()){draft_.target=file;draft_.source=Source::Exe;draft_.name=utf8(std::filesystem::path(wide(file)).stem().wstring());draft_.directory=utf8(std::filesystem::path(wide(file)).parent_path().wstring());choose=true;}
        }
    }else {
        secondary(pickerTab_==0?"Apps with visible windows. Background services and tray-only helpers are excluded.":pickerTab_==2?"Choose an installed Steam game or tool. Steam handles the launch.":"Launchable Windows apps and Start Menu entries. Use Browse EXE if an app is missing.");
        ImGui::SetNextItemWidth(-115);input("##Search",search_);ImGui::SameLine();ImGui::BeginDisabled(discovery_.valid());if(ImGui::Button("Refresh",{100,0}))scan(pickerTab_);ImGui::EndDisabled();
        if(search_.empty())tooltip("Search applications by name");
        if(ImGui::BeginChild("Choices",{0,-95},ImGuiChildFlags_Borders)) {
            if(discovery_.valid())secondary("Finding applications...");
            else if(choices_.empty())secondary("No apps found. Refresh or choose another source.");
            for(size_t i=0;i<choices_.size();++i) {
                auto& app=choices_[i];std::string hay=app.name+" "+app.description,needle=search_;
                std::transform(hay.begin(),hay.end(),hay.begin(),[](unsigned char c){return char(std::tolower(c));});std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return char(std::tolower(c));});
                if(!needle.empty()&&hay.find(needle)==std::string::npos)continue;
                ImGui::PushID(int(i));if(ImGui::Selectable(app.name.c_str(),false,0,{0,25})) {
                    draft_.name=app.name;draft_.source=app.source;draft_.target=app.target;draft_.directory=app.directory;draft_.probe=app.probe;
                    draft_.readiness=app.source==Source::Steam&&app.probe.empty()?Readiness::Manual:Readiness::Process;choose=true;
                }tooltip(app.description+"\n"+app.target);ImGui::PopID();
            }
        }ImGui::EndChild();
        if(pickerTab_==2) {
            ImGui::SetNextItemWidth(-170);input("##AppID",draft_.target);ImGui::SameLine();
            if(ImGui::Button("Use App ID / URL")) {
                std::string value=draft_.target;auto pos=value.find("/app/");if(pos!=std::string::npos){value=value.substr(pos+5);value=value.substr(0,value.find('/'));}
                if(!value.empty()&&value.find_first_not_of("0123456789")==std::string::npos){draft_.target=value;draft_.source=Source::Steam;draft_.name="Steam app "+value;draft_.readiness=Readiness::Manual;choose=true;}
                else note("Enter a numeric Steam App ID or a Steam store app URL.");
            }
        }
    }
    if(ImGui::Button("Cancel")){pickerOpen_=false;ImGui::CloseCurrentPopup();}
    if(choose){pickerOpen_=false;editOpen_=true;ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
    if(choose)ImGui::OpenPopup("App details");
}
void Ui::taskEditor() {
    ImGui::SetNextWindowSize({710,0},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("App details",&editOpen_,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::TextColored(accent,"%s",sourceName(draft_.source));ImGui::SetNextItemWidth(-1);input("Name",draft_.name);
    ImGui::SetNextItemWidth(-1);input(draft_.source==Source::Steam?"Steam App ID":draft_.source==Source::Installed?"Windows App ID":"Executable",draft_.target);
    if(draft_.source==Source::Exe&&ImGui::Button("Browse...")){auto file=browseExe(window_);if(!file.empty())draft_.target=file;}
    if(ImGui::CollapsingHeader("Startup and detection",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Ready when");ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##ReadyWhen",readinessLabel(draft_.readiness))) {
            for(int k=0;k<4;++k)if(ImGui::Selectable(readinessLabel(Readiness(k)),int(draft_.readiness)==k))draft_.readiness=Readiness(k);ImGui::EndCombo();
        }
        if(draft_.source!=Source::Exe) {
            secondary("Steam and Windows apps are launch-only in this preview. Stop leaves them open.");
            if(draft_.source==Source::Steam) {
                ImGui::SetNextItemWidth(-100);input("##Probe",draft_.probe);ImGui::SameLine();if(ImGui::Button("Detect EXE")){auto file=browseExe(window_);if(!file.empty())draft_.probe=file;}
                secondary("Optional: identify the app's process for automatic readiness. Otherwise choose Confirm ready manually.");
            }
        }
        ImGui::SetNextItemWidth(150);seconds("Startup timeout",draft_.timeoutMs,1000);
        ImGui::SetNextItemWidth(150);seconds("Settle after detection",draft_.settleMs);
        if(draft_.timing==Timing::Delay){ImGui::SetNextItemWidth(150);seconds("Delay after previous",draft_.delayMs);}
        if(draft_.readiness==Readiness::Completion)secondary("One-shot helper: require exit code 0. Verify that your helper actually uses this success code.");
        if(draft_.readiness==Readiness::Manual)secondary("The row will show Confirm ready. Use it only after the app or headset is ready.");
    }
    if(ImGui::CollapsingHeader("Arguments and working folder")) {
        ImGui::BeginDisabled(draft_.source==Source::Steam);ImGui::SetNextItemWidth(-1);input("Arguments",draft_.arguments);ImGui::EndDisabled();
        if(draft_.source==Source::Steam)secondary("Set launch arguments in Steam's own Properties dialog.");
        ImGui::SetNextItemWidth(-1);input("Working folder",draft_.directory);
    }
    ImGui::Separator();bool valid=!draft_.name.empty()&&!draft_.target.empty();
    if(draft_.source==Source::Steam)valid=valid&&draft_.target.find_first_not_of("0123456789")==std::string::npos;
    if(draft_.source==Source::Steam&&draft_.probe.empty()&&draft_.readiness!=Readiness::Manual){valid=false;secondary("Choose Confirm ready manually, or set a detection EXE.");}
    ImGui::BeginDisabled(!valid);if(button(editTask_.empty()?"Add app":"Save app",{145,38},{.14f,.29f,.34f,1})) {
        for(auto& g:selected().groups)if(g.id==editGroup_) {
            if(editTask_.empty())g.tasks.push_back(draft_);else for(auto& t:g.tasks)if(t.id==editTask_)t=draft_;
        }dirty_=true;editOpen_=false;ImGui::CloseCurrentPopup();
    }ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel",{100,38})){editOpen_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
}
void Ui::profileEditor() {
    ImGui::SetNextWindowSize({450,0},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Profile",&profileOpen_,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::SetNextItemWidth(-1);input("Profile name",profileName_);
    ImGui::BeginDisabled(profileName_.empty());if(ImGui::Button("Create empty")){Profile p{newId(),profileName_,false,{{newId(),"Launch apps",{},{}}}};settings_.profiles.push_back(p);settings_.selected=p.id;dirty_=true;profileOpen_=false;ImGui::CloseCurrentPopup();}
    ImGui::SameLine();if(ImGui::Button("Duplicate current")){auto p=duplicate(selected(),profileName_);settings_.profiles.push_back(p);settings_.selected=p.id;dirty_=true;profileOpen_=false;ImGui::CloseCurrentPopup();}
    ImGui::EndDisabled();if(ImGui::Button("Cancel")){profileOpen_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
}
void Ui::groupEditor() {
    ImGui::SetNextWindowSize({460,0},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Group",&groupOpen_,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::SetNextItemWidth(-1);input("Group name",groupName_);ImGui::BeginDisabled(groupName_.empty());
    if(ImGui::Button("Save group")) {
        if(groupId_.empty())selected().groups.push_back({newId(),groupName_,{},{}});
        else for(auto& g:selected().groups)if(g.id==groupId_)g.name=groupName_;
        dirty_=true;groupOpen_=false;ImGui::CloseCurrentPopup();
    }ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel")){groupOpen_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
}
void Ui::render() {
    auto& io=ImGui::GetIO();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("ReadyForLaunch",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    auto& fonts=io.Fonts->Fonts;ImGui::PushFont(fonts.Size>1?fonts[1]:nullptr,34);ImGui::TextColored(accent,"ReadyForLaunch");ImGui::PopFont();
    if(ImGui::IsItemClicked())ShellExecuteW(window_,L"open",L"https://github.com/AdamChesters/ReadyForLaunch",nullptr,nullptr,SW_SHOWNORMAL);
    ImGui::SameLine();ImGui::SetCursorPosY(ImGui::GetCursorPosY()+14);ImGui::TextColored(muted,"by Adam Chesters");
    right(110);if(ImGui::Button("Settings",{110,36})){settingsOpen_=true;ImGui::OpenPopup("Settings");}
    bool active=engine_.active();
    ImGui::Spacing();ImGui::BeginDisabled(active);
    for(auto& p:settings_.profiles)if(p.builtin) {
        if(button(p.name.c_str(),{125,36},p.id==settings_.selected?ImVec4(.14f,.29f,.34f,1):ImVec4(.055f,.07f,.08f,1))){settings_.selected=p.id;dirty_=true;}
        ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(190);const char* custom=selected().builtin?"Custom profiles":selected().name.c_str();
    if(ImGui::BeginCombo("##Profiles",custom)){for(auto& p:settings_.profiles)if(!p.builtin&&ImGui::Selectable(p.name.c_str(),p.id==settings_.selected)){settings_.selected=p.id;dirty_=true;}ImGui::EndCombo();}
    ImGui::SameLine();if(ImGui::Button("+##Profile",{36,36})){profileName_="New profile";profileOpen_=true;ImGui::OpenPopup("Profile");}ImGui::EndDisabled();
    auto& p=selected();ImGui::Spacing();ImGui::PushFont(nullptr,23);ImGui::TextUnformatted("Launch stack");ImGui::PopFont();
    right(130);ImGui::BeginDisabled(active);if(ImGui::Button("+ Add group",{130,34})){groupId_.clear();groupName_="New group";groupOpen_=true;ImGui::OpenPopup("Group");}ImGui::EndDisabled();
    secondary("Groups run together, or follow another group. Apps keep their own sequence.");
    float footer=102;if(!message_.empty())footer+=42;
    ImGui::BeginChild("Stack",{0,-footer},ImGuiChildFlags_None);
    std::string addGroup,editGroup,removeGroup;
    for(auto& g:p.groups) {
        ImGui::PushID(g.id.c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{13,10});
        ImGui::BeginChild("GroupCard",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY);
        ImGui::PushFont(nullptr,20);ImGui::TextUnformatted(g.name.c_str());ImGui::PopFont();ImGui::SameLine();
        ImGui::PushFont(nullptr,14);ImGui::TextColored(muted,"%s",g.afterGroup.empty()?"Starts on GO":("After "+groupName(p,g.afterGroup)).c_str());ImGui::PopFont();
        right(147);ImGui::BeginDisabled(active);
        if(ImGui::Button("+ Add app",{110,30}))addGroup=g.id;ImGui::SameLine();if(ImGui::SmallButton("..."))ImGui::OpenPopup("Group actions");
        if(ImGui::BeginPopup("Group actions")) {
            if(ImGui::MenuItem("Rename group"))editGroup=g.id;
            bool referenced=std::any_of(p.groups.begin(),p.groups.end(),[&](auto& other){return other.afterGroup==g.id;});
            if(ImGui::MenuItem("Delete empty group",nullptr,false,g.tasks.empty()&&!referenced))removeGroup=g.id;
            tooltip("Move or remove apps first. Groups referenced by another group cannot be deleted.");ImGui::EndPopup();
        }ImGui::EndDisabled();ImGui::Separator();
        if(g.tasks.empty())secondary("Add an app from your running windows, EXE, Steam, or Windows app list.");
        else rows(p,g,active);
        ImGui::EndChild();ImGui::PopStyleVar();
        if(!active&&ImGui::BeginDragDropTarget()){if(auto payload=ImGui::AcceptDragDropPayload("RFL_TASK"))moveTask(static_cast<const char*>(payload->Data),g.id);ImGui::EndDragDropTarget();}
        ImGui::PopID();ImGui::Spacing();
    }
    ImGui::BeginDisabled(active);if(ImGui::Button("+ Add app",{130,34})) {
        if(p.groups.empty()){p.groups.push_back({newId(),"Launch apps",{},{}});dirty_=true;}addGroup=p.groups.back().id;
    }ImGui::EndDisabled();ImGui::SameLine();secondary("Running apps  /  EXE  /  Steam  /  Installed apps");
    ImGui::EndChild();
    if(!addGroup.empty())startEdit(addGroup);
    if(!editGroup.empty()){groupId_=editGroup;groupName_=groupName(p,editGroup);groupOpen_=true;ImGui::OpenPopup("Group");}
    if(!removeGroup.empty()){std::erase_if(p.groups,[&](auto& g){return g.id==removeGroup;});dirty_=true;}
    ImGui::Separator();
    if(!message_.empty()){ImGui::TextWrapped("%s",message_.c_str());if(ImGui::IsItemClicked())message_.clear();}
    int count=0;for(auto& g:p.groups)for(auto& t:g.tasks)if(t.enabled)++count;
    ImGui::AlignTextToFramePadding();ImGui::TextColored(muted,"%d apps  ·  %zu groups  ·  %s",count,p.groups.size(),active?(engine_.stopping()?"Stopping":"Session active"):"Ready");
    right(427);ImGui::BeginDisabled(!active);
    if(button("Emergency stop",{160,43},{.24f,.08f,.09f,1})){stop(true);note("Emergency stop requested for directly launched apps.");}
    tooltip("Force-terminate this session's directly launched processes. Unsaved app data may be lost.");ImGui::SameLine();
    if(ImGui::Button("Stop",{100,43})){stop(false);note("Closing session-owned apps. Existing and launch-only apps are left open.");}ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(active);ImGui::PushStyleColor(ImGuiCol_Text,{.02f,.07f,.09f,1});
    if(button("GO",{145,43},accent)) {
        auto errors=preview_?std::vector<std::string>{"Preview mode does not launch apps."}:engine_.begin(p);
        if(errors.empty()){activeProfile_=p.id;note("Starting "+p.name);engine_.tick(GetTickCount64());}
        else {std::string text;for(auto& error:errors)text+=error+"\n";note(text);}
    }ImGui::PopStyleColor();ImGui::EndDisabled();
    ImGui::PushFont(nullptr,12);ImGui::TextColored(muted,"0.1.0 preview");ImGui::PopFont();
    if(!captureView_.empty()) {
        if(captureView_=="picker"&&!p.groups.empty())startEdit(p.groups.front().id);
        if(captureView_=="editor"&&!p.groups.empty()&&!p.groups.front().tasks.empty())startEdit(p.groups.front().id,p.groups.front().tasks.front().id);
        if(captureView_=="settings"){settingsOpen_=true;ImGui::OpenPopup("Settings");}
        if(captureView_=="group"){groupOpen_=true;groupName_="New group";groupId_.clear();ImGui::OpenPopup("Group");}
        captureView_.clear();
    }
    appPicker();taskEditor();profileEditor();groupEditor();
    ImGui::SetNextWindowSize({600,430},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Settings",&settingsOpen_)) {
        ImGui::TextUnformatted("ReadyForLaunch 0.1.0");secondary("by Adam Chesters · Windows preview");
        if(ImGui::Button("Open settings folder")){std::filesystem::create_directories(directory_);ShellExecuteW(window_,L"open",directory_.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        secondary("Settings save automatically. Existing apps and Steam / Windows brokered apps remain open on Stop.");
        ImGui::SeparatorText("Current profile");ImGui::BeginDisabled(active);
        if(input("Name",p.name))dirty_=true;
        if(ImGui::Button("Duplicate profile")){profileName_=p.name+" copy";profileOpen_=true;settingsOpen_=false;ImGui::CloseCurrentPopup();}
        ImGui::BeginDisabled(p.builtin);if(ImGui::Button("Delete custom profile"))ImGui::OpenPopup("Delete profile?");ImGui::EndDisabled();
        if(ImGui::BeginPopupModal("Delete profile?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Delete this profile and its launch configuration?");
            if(ImGui::Button("Delete")){auto id=p.id;settings_.selected=settings_.profiles.front().id;std::erase_if(settings_.profiles,[&](auto& profile){return profile.id==id;});dirty_=true;ImGui::CloseCurrentPopup();}
            ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }ImGui::EndDisabled();ImGui::SeparatorText("Recent activity");
        ImGui::BeginChild("Activity",{0,-40});for(auto& line:log_)ImGui::TextWrapped("%s",line.c_str());for(auto& n:engine_.nodes())ImGui::TextWrapped("%s: %s — %s",n.task.name.c_str(),phaseName(n.phase),n.detail.c_str());ImGui::EndChild();
        if(ImGui::Button("Close")){settingsOpen_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(profileOpen_&&!ImGui::IsPopupOpen("Profile"))ImGui::OpenPopup("Profile");
    if(editOpen_&&!ImGui::IsPopupOpen("App details"))ImGui::OpenPopup("App details");
    if(dirty_)save();ImGui::End();
}
}
