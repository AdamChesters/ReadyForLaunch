#include "model.hpp"
#include <windows.h>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <random>
#include <sstream>

namespace rfl {
std::string newId() {
    static std::mt19937_64 random(std::random_device{}());
    std::ostringstream out; out << std::hex << random() << random(); return out.str();
}
const char* sourceName(Source s) { return s==Source::Exe?"EXE":s==Source::Steam?"STEAM":s==Source::Squirrel?"APP":"INSTALLED"; }
Settings defaults() {
    Settings s;
    for (auto name : {"DCS World","MSFS 2024","MSFS 2020","X-Plane 12","IL-2"}) {
        Profile p{newId(),name,true,{}};
        p.groups.push_back({newId(),"Launch apps",{}, {}});
        s.profiles.push_back(p);
    }
    s.selected=s.profiles.front().id; return s;
}
Profile duplicate(const Profile& p, std::string name) {
    auto result=p; result.id=newId(); result.name=std::move(name); result.builtin=false;
    std::unordered_map<std::string,std::string> ids;
    for(auto& g:result.groups) { auto old=g.id; g.id=newId(); ids[old]=g.id; for(auto& t:g.tasks)t.id=newId(); }
    for(auto& g:result.groups)if(!g.afterGroup.empty()&&ids.contains(g.afterGroup))g.afterGroup=ids.at(g.afterGroup);
    return result;
}
void copyConfiguration(Profile& destination,const Profile& source) {
    auto copy=duplicate(source,destination.name);destination.groups=std::move(copy.groups);
}
bool canFollow(const Profile& p,const std::string& group,const std::string& predecessor) {
    auto id=predecessor; std::unordered_set<std::string> seen;
    while(!id.empty()) {
        if(id==group||!seen.insert(id).second)return false;
        auto it=std::find_if(p.groups.begin(),p.groups.end(),[&](auto& g){return g.id==id;});
        if(it==p.groups.end())return false;
        id=it->afterGroup;
    }
    return true;
}
std::vector<std::string> validate(const Profile& p) {
    std::vector<std::string> errors; std::unordered_set<std::string> ids; int enabled=0;
    for(const auto& g:p.groups) {
        if(g.id.empty()||!ids.insert(g.id).second)errors.push_back("Duplicate or missing group ID");
        if(!g.afterGroup.empty()) {
            if(!canFollow(p,g.id,g.afterGroup))errors.push_back(g.name+": invalid or circular group dependency");
            auto it=std::find_if(p.groups.begin(),p.groups.end(),[&](auto& x){return x.id==g.afterGroup;});
            if(it!=p.groups.end()&&std::none_of(it->tasks.begin(),it->tasks.end(),[](auto& t){return t.enabled;}))errors.push_back(g.name+": dependency group has no enabled apps");
        }
        const Task* previous=nullptr;
        for(const auto& t:g.tasks) {
            if(t.id.empty()||!ids.insert(t.id).second)errors.push_back("Duplicate or missing task ID");
            if(!t.enabled)continue;
            ++enabled;
            if(t.name.empty()||t.target.empty())errors.push_back(g.name+": choose a launch target for "+t.name);
            if(t.source==Source::Steam&&(t.target.empty()||t.target.find_first_not_of("0123456789")!=std::string::npos))errors.push_back(t.name+": Steam App ID must contain digits only");
            if(t.source==Source::Squirrel&&(t.application.empty()||t.application.find_first_of("/\\:")!=std::string::npos))errors.push_back(t.name+": invalid application filename");
            if(t.delayMs<0||t.delayMs>3600000||t.timeoutMs<1000||t.timeoutMs>3600000||t.settleMs<0||t.settleMs>3600000)errors.push_back(t.name+": timing is outside the supported range");
            if(previous&&t.timing==Timing::Finished&&previous->readiness!=Readiness::Completion)errors.push_back(t.name+": set the previous app's readiness to Successful completion");
            previous=&t;
        }
    }
    if(!enabled)errors.push_back("Add or enable at least one app before GO.");
    return errors;
}
Json serialize(const Settings& s) {
    Json root={{"schemaVersion",2},{"selected",s.selected},{"profiles",Json::array()}};
    for(auto& p:s.profiles) {
        Json jp={{"id",p.id},{"name",p.name},{"builtin",p.builtin},{"groups",Json::array()}};
        for(auto& g:p.groups) {
            Json jg={{"id",g.id},{"name",g.name},{"afterGroup",g.afterGroup},{"tasks",Json::array()}};
            for(auto& t:g.tasks)jg["tasks"].push_back({{"id",t.id},{"name",t.name},{"source",int(t.source)},{"target",t.target},{"arguments",t.arguments},{"directory",t.directory},{"probe",t.probe},{"application",t.application},{"timing",int(t.timing)},{"readiness",int(t.readiness)},{"enabled",t.enabled},{"delayMs",t.delayMs},{"timeoutMs",t.timeoutMs},{"settleMs",t.settleMs}});
            jp["groups"].push_back(jg);
        } root["profiles"].push_back(jp);
    } return root;
}
Settings deserialize(const Json& j) {
    const auto schema=j.at("schemaVersion").get<int>();
    if(schema!=1&&schema!=2)throw std::runtime_error("Unsupported settings version");
    Settings s; s.selected=j.value("selected","");
    for(auto& jp:j.at("profiles")) {
        Profile p{jp.at("id"),jp.at("name"),jp.value("builtin",false),{}};
        for(auto& jg:jp.at("groups")) {
            Group g{jg.at("id"),jg.at("name"),jg.value("afterGroup",""),{}};
            for(auto& jt:jg.at("tasks")) {
                Task t; t.id=jt.at("id"); t.name=jt.at("name");t.target=jt.at("target");
                int source=jt.value("source",0),timing=jt.value("timing",1),ready=jt.value("readiness",0);
                if(source<0||source>3||timing<0||timing>4||ready<0||ready>3)throw std::runtime_error("Unknown task option");
                t.source=Source(source);t.timing=Timing(timing);t.readiness=Readiness(ready);
                t.arguments=jt.value("arguments","");t.directory=jt.value("directory","");t.probe=jt.value("probe","");
                t.application=jt.value("application","");
                t.enabled=jt.value("enabled",true);t.delayMs=jt.value("delayMs",5000);t.timeoutMs=jt.value("timeoutMs",60000);t.settleMs=jt.value("settleMs",1000);
                g.tasks.push_back(t);
            }p.groups.push_back(g);
        }s.profiles.push_back(p);
    }
    if(s.profiles.empty())throw std::runtime_error("No profiles in settings");
    if(std::none_of(s.profiles.begin(),s.profiles.end(),[&](auto& p){return p.id==s.selected;}))s.selected=s.profiles.front().id;
    return s;
}
Settings loadSettings(const std::filesystem::path& path,std::string& warning) {
    if(!std::filesystem::exists(path))return defaults();
    try { std::ifstream f(path); return deserialize(Json::parse(f)); }
    catch(const std::exception& e) {
        warning=std::string("Settings could not be read: ")+e.what();
        auto backup=path;backup+=L".bak";
        try { std::ifstream f(backup);auto s=deserialize(Json::parse(f));warning+=". Loaded last-good backup.";return s; }
        catch(...) {throw std::runtime_error(warning+". The original file was preserved; repair it or use a different --data-dir.");}
    }
}
void saveSettings(const std::filesystem::path& path,const Settings& s) {
    std::filesystem::create_directories(path.parent_path());
    auto temp=path;temp+=L".tmp"; auto backup=path;backup+=L".bak";
    {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<serialize(s).dump(2);f.flush();if(!f)throw std::runtime_error("Could not write settings");}
    if(std::filesystem::exists(path)) {
        // Never replace a valid backup with malformed settings after recovery.
        try {std::ifstream f(path);(void)deserialize(Json::parse(f));std::filesystem::copy_file(path,backup,std::filesystem::copy_options::overwrite_existing);}catch(...){}
    }
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace settings file");
}
}
