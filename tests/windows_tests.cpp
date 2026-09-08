#include "windows.hpp"
#include <iostream>
#include <stdexcept>
#include <shobjidl.h>
#include <wrl/client.h>

using namespace rfl;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool until(const std::function<bool()>& predicate,int timeout=5000){auto start=GetTickCount64();while(GetTickCount64()-start<DWORD(timeout)){if(predicate())return true;Sleep(25);}return false;}
struct FixtureCleanup {
    WindowsBackend& backend;std::vector<int> tickets;
    ~FixtureCleanup(){for(auto ticket:tickets)if(backend.ownedAlive(ticket))backend.close(ticket,true);}
};
int main(int argc,char** argv) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {
        check(argc==2,"fixture path required");Task t;t.id="fixture";t.name="Fixture";t.target=argv[1];t.settleMs=0;
        Microsoft::WRL::ComPtr<IApplicationActivationManager> activation;
        check(SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&activation))),"Windows packaged-app activation provider available");
        WindowsBackend backend;FixtureCleanup cleanup{backend};
        auto launch=[&](const Task& entry){auto result=backend.start(entry);if(result.ticket>=0)cleanup.tickets.push_back(result.ticket);return result;};
        auto owned=launch(t);check(owned.error.empty()&&owned.managed,"direct launch is tracked");
        check(until([&]{return backend.poll(owned.ticket).alive;}),"process becomes alive");
        auto listed=[&]{auto apps=runningApps();return std::count_if(apps.begin(),apps.end(),[&](auto& app){std::error_code ec;return app.source==Source::Exe&&std::filesystem::equivalent(wide(app.target),wide(t.target),ec);});};
        check(listed()==0,"hidden helper excluded from running apps");
        WindowsBackend other;auto reused=other.start(t);check(reused.reused&&!reused.managed,"pre-existing process is reused");
        check(!other.close(reused.ticket,true).empty(),"refuses killing reused process");
        Sleep(200);check(other.close(reused.ticket,false).empty(),"graceful close is sent to an existing selected instance");check(until([&]{return !backend.ownedAlive(owned.ticket);}),"graceful process exit");
        t.arguments="--refuse --child";auto tree=launch(t);check(tree.managed,"child tree tracked");Sleep(300);
        check(backend.close(tree.ticket,false).empty(),"stubborn app gets close request");Sleep(100);check(backend.ownedAlive(tree.ticket),"refusal leaves process running");
        check(backend.close(tree.ticket,true).empty(),"test cleanup terminates only its own fixture job");check(until([&]{return !backend.ownedAlive(tree.ticket);}),"entire fixture tree ended");
        t.arguments="--visible";auto visible=launch(t);check(visible.managed,"visible app launch tracked");check(until([&]{return listed()==1;}),"visible app included once in running picker");backend.close(visible.ticket,true);check(until([&]{return !backend.ownedAlive(visible.ticket);}),"visible fixture ended");
        t.arguments="--visible --tool";auto tool=launch(t);check(tool.managed,"tool fixture launched");Sleep(200);check(listed()==0,"tool/helper window excluded from running picker");backend.close(tool.ticket,true);check(until([&]{return !backend.ownedAlive(tool.ticket);}),"tool fixture ended");
        t.arguments="--success";auto helper=launch(t);check(helper.error.empty(),"one-shot launches");check(until([&]{return backend.poll(helper.ticket).exited;}),"helper completed");check(backend.poll(helper.ticket).exitCode==0,"success exit code observed");
        t.target="C:/not-a-real-folder/no-such-app.exe";check(!backend.start(t).error.empty(),"missing EXE reported");
        auto directory=std::filesystem::temp_directory_path()/wide("ReadyForLaunch-tests-"+newId());auto settings=defaults();std::string warning;
        saveSettings(directory/L"settings.json",settings);auto loaded=loadSettings(directory/L"settings.json",warning);check(serialize(loaded)==serialize(settings),"atomic settings roundtrip");
        settings.profiles[0].name="Changed";saveSettings(directory/L"settings.json",settings);check(std::filesystem::exists(directory/L"settings.json.bak"),"last-good backup exists");
        // Emulate a Squirrel-style stable launcher and a replaced version folder.
        auto root=directory/L"VersionedApp",v1=root/L"app-1.0.0",v2=root/L"app-2.0.0";
        std::filesystem::create_directories(v1);
        std::filesystem::copy_file(wide(argv[1]),root/L"Update.exe");std::filesystem::copy_file(wide(argv[1]),v1/L"Fixture.exe");
        Task legacy;legacy.id="versioned";legacy.name="Versioned fixture";legacy.target=utf8((v1/L"Fixture.exe").wstring());legacy.settleMs=0;
        Task stable=legacy;check(normalizeLaunchTarget(stable),"version-specific EXE resolves to stable launcher");
        check(stable.source==Source::Squirrel&&stable.application=="Fixture.exe"&&targetFolder(stable)==root,"stable launcher metadata and target folder");
        auto first=launch(stable);check(first.error.empty(),"stable launcher starts");
        check(until([&]{return backend.poll(first.ticket).window;}),"actual child window observed after launcher exits");
        check(observeApps({stable})[stable.id],"idle light observes application rather than exited launcher");
        auto duplicate=other.start(stable);check(duplicate.reused,"stable app already-running check avoids another launch");
        check(other.close(duplicate.ticket,false).empty(),"stable app receives graceful close");
        check(until([&]{return !backend.ownedAlive(first.ticket);}),"first version exits");
        std::filesystem::remove(v1/L"Fixture.exe");std::filesystem::remove(v1);std::filesystem::create_directories(v2);
        std::filesystem::copy_file(wide(argv[1]),v2/L"Fixture.exe");
        auto updated=launch(legacy);check(updated.error.empty(),"old saved version path upgrades automatically");
        check(until([&]{return backend.poll(updated.ticket).window;}),"replacement version is detected");
        backend.close(updated.ticket,false);check(until([&]{return !backend.ownedAlive(updated.ticket);}),"replacement version closes gracefully");
        std::filesystem::remove(v2/L"Fixture.exe");std::filesystem::remove(v2);std::filesystem::remove(root/L"Update.exe");std::filesystem::remove(root);
        // Remove only the three explicit fixture files, never a recursive computed tree.
        std::filesystem::remove(directory/L"settings.json");std::filesystem::remove(directory/L"settings.json.bak");std::filesystem::remove(directory);
        std::cout<<"Windows lifecycle and app-window filtering passed. Discovery: "<<runningApps().size()<<" running apps, "<<steamApps().size()<<" Steam apps, "<<installedApps().size()<<" installed entries.\n";
        CoUninitialize();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";CoUninitialize();return 1;}
}
