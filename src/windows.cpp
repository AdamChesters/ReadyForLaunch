#include "windows.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propsys.h>
#include <dwmapi.h>
#include <appmodel.h>
#include <tlhelp32.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <regex>
#include <cwctype>
#include <bcrypt.h>
#include <iomanip>

namespace rfl {
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
struct Handle {
    HANDLE h=nullptr;
    Handle()=default;explicit Handle(HANDLE value):h(value){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(Handle&& other)noexcept:h(other.h){other.h=nullptr;}
    Handle& operator=(Handle&& other)noexcept {if(this!=&other){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=other.h;other.h=nullptr;}return *this;}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::wstring wide(const std::string& v) {
    if(v.empty())return {};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),int(v.size()),nullptr,0);
    if(!count)throw std::runtime_error("Invalid UTF-8 text");std::wstring out(count,0);MultiByteToWideChar(CP_UTF8,0,v.data(),int(v.size()),out.data(),count);return out;
}
std::string utf8(const std::wstring& v) {
    if(v.empty())return {};int count=WideCharToMultiByte(CP_UTF8,0,v.data(),int(v.size()),nullptr,0,nullptr,nullptr);
    std::string out(count,0);WideCharToMultiByte(CP_UTF8,0,v.data(),int(v.size()),out.data(),count,nullptr,nullptr);return out;
}
std::string systemError(DWORD code) {
    wchar_t* message=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,reinterpret_cast<wchar_t*>(&message),0,nullptr);
    std::string result=message?utf8(message):"Windows error "+std::to_string(code);if(message)LocalFree(message);return result;
}
fs::path dataDirectory() {
    PWSTR path=nullptr; if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path)))throw std::runtime_error("Local AppData unavailable");
    fs::path result(path);CoTaskMemFree(path);return result/L"ReadyForLaunch";
}
fs::path appDirectory() {wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return fs::path(path).parent_path();}
std::string sha256(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read update file");
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 unavailable");
    DWORD size=0,count=0;bool ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&count,0)>=0;
    std::vector<unsigned char> object(size);unsigned char digest[32]{};
    ok=ok&&BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
    std::array<char,65536> buffer{};
    while(ok&&file){file.read(buffer.data(),buffer.size());auto bytes=file.gcount();if(bytes)ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),ULONG(bytes),0)>=0;}
    ok=ok&&!file.bad()&&BCryptFinishHash(hash,digest,32,0)>=0;
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
    if(!ok)throw std::runtime_error("SHA256 verification failed");std::ostringstream output;
    for(auto byte:digest)output<<std::hex<<std::setfill('0')<<std::setw(2)<<int(byte);return output.str();
}
std::wstring quote(const std::wstring& argument) {
    std::wstring result=L"\"";size_t slashes=0;
    for(auto c:argument) {
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;slashes=0;continue;}
        result.append(slashes,L'\\');slashes=0;result+=c;
    }result.append(slashes*2,L'\\');result+=L'\"';return result;
}
std::string browseExe(HWND owner) {
    ComPtr<IFileOpenDialog> picker;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&picker))))return {};
    COMDLG_FILTERSPEC types[]={{L"Applications (*.exe)",L"*.exe"}};picker->SetFileTypes(1,types);picker->SetTitle(L"Choose an application");
    if(FAILED(picker->Show(owner)))return {};ComPtr<IShellItem> item;picker->GetResult(&item);PWSTR path=nullptr;
    if(!item||FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path)))return {};auto result=utf8(path);CoTaskMemFree(path);return result;
}
static std::wstring processPath(HANDLE process) {
    std::wstring path(32768,0);DWORD size=DWORD(path.size());if(!QueryFullProcessImageNameW(process,0,path.data(),&size))return {};path.resize(size);return path;
}
static std::wstring identityPath(const std::wstring& path) {
    if(path.empty())return {};std::error_code ec;auto canonical=fs::weakly_canonical(path,ec).wstring();if(ec)canonical=path;
    for(auto& c:canonical)c=wchar_t(towlower(c));return canonical;
}
static std::string appId(DWORD pid) {
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));if(!process.h)return {};
    UINT32 size=0; if(GetApplicationUserModelId(process.h,&size,nullptr)!=ERROR_INSUFFICIENT_BUFFER)return {};
    std::wstring id(size,0);if(GetApplicationUserModelId(process.h,&size,id.data())!=ERROR_SUCCESS)return {};if(!id.empty()&&id.back()==0)id.pop_back();return utf8(id);
}
static bool eligibleWindow(HWND window) {
    if(!IsWindowVisible(window)||GetWindow(window,GW_OWNER))return false;
    LONG_PTR style=GetWindowLongPtrW(window,GWL_EXSTYLE);
    if(style&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))return false;
    DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return false;
    wchar_t title[1024];if(!GetWindowTextW(window,title,1024))return false;
    wchar_t cls[128];GetClassNameW(window,cls,128);
    return wcscmp(cls,L"Shell_TrayWnd")&&wcscmp(cls,L"Shell_SecondaryTrayWnd")&&wcscmp(cls,L"Progman")&&wcscmp(cls,L"WorkerW");
}
static std::vector<DWORD> allPids() {
    std::vector<DWORD> result;Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W entry{sizeof(entry)};
    if(snapshot.h!=INVALID_HANDLE_VALUE&&Process32FirstW(snapshot.h,&entry))do{result.push_back(entry.th32ProcessID);}while(Process32NextW(snapshot.h,&entry));return result;
}
static HANDLE existingProcess(const std::string& path) {
    if(path.empty())return nullptr;auto target=identityPath(wide(path));
    for(auto pid:allPids()) {
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        if(process.h&&WaitForSingleObject(process.h,0)==WAIT_TIMEOUT&&identityPath(processPath(process.h))==target){auto handle=process.h;process.h=nullptr;return handle;}
    }return nullptr;
}
bool normalizeLaunchTarget(Task& task) {
    if(task.source!=Source::Exe)return false;
    fs::path executable=wide(task.target),version=executable.parent_path();
    if(version.filename().wstring().rfind(L"app-",0)!=0)return false;
    auto root=version.parent_path(),launcher=root/L"Update.exe";
    std::error_code ec;if(!fs::is_regular_file(launcher,ec))return false;
    auto extra=task.arguments;task.source=Source::Squirrel;task.application=utf8(executable.filename().wstring());
    task.target=utf8(launcher.wstring());task.directory=utf8(root.wstring());task.probe.clear();
    task.arguments="--processStart "+utf8(quote(executable.filename().wstring()));
    if(!extra.empty())task.arguments+=" --process-start-args "+utf8(quote(wide(extra)));return true;
}
static void normalizeChoice(AppChoice& app) {
    Task task;task.source=app.source;task.target=app.target;task.arguments=app.arguments;task.directory=app.directory;
    if(normalizeLaunchTarget(task)){app.source=task.source;app.target=task.target;app.arguments=task.arguments;app.directory=task.directory;app.application=task.application;app.description="App launcher · "+app.application;}
}
static std::unordered_set<DWORD> visiblePids() {
    std::unordered_set<DWORD> ids;EnumWindows([](HWND window,LPARAM data)->BOOL {
        if(eligibleWindow(window)){DWORD pid=0;GetWindowThreadProcessId(window,&pid);reinterpret_cast<std::unordered_set<DWORD>*>(data)->insert(pid);}return TRUE;
    },reinterpret_cast<LPARAM>(&ids));return ids;
}
static HANDLE targetProcess(const Task& task) {
    if(task.source==Source::Exe)return existingProcess(task.target);
    if(!task.probe.empty()&&task.source!=Source::Squirrel)return existingProcess(task.probe);
    auto visible=visiblePids();Handle fallback;
    auto root=task.source==Source::Squirrel?fs::path(wide(task.target)).parent_path().wstring():wide(task.directory);
    auto prefix=root.empty()?std::wstring{}:identityPath(root)+L"\\";
    for(auto pid:allPids()) {
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        if(!process.h||WaitForSingleObject(process.h,0)!=WAIT_TIMEOUT)continue;
        bool match=false;
        if(task.source==Source::Installed)match=appId(pid)==task.target;
        else {
            auto path=identityPath(processPath(process.h));
            if(!prefix.empty()&&path.rfind(prefix,0)==0) {
                if(task.source==Source::Squirrel)match=_wcsicmp(fs::path(path).filename().c_str(),wide(task.application).c_str())==0;
                else if(task.source==Source::Steam)match=visible.contains(pid);
            }
        }
        if(!match)continue;
        if(visible.contains(pid)){auto h=process.h;process.h=nullptr;return h;}
        if(!fallback.h)fallback=std::move(process);
    }
    auto h=fallback.h;fallback.h=nullptr;return h;
}
std::unordered_map<std::string,bool> observeApps(const std::vector<Task>& tasks) {
    std::unordered_map<std::string,bool> result;
    for(auto task:tasks){normalizeLaunchTarget(task);Handle process(targetProcess(task));result[task.id]=process.h!=nullptr;}return result;
}
fs::path targetFolder(const Task& task) {
    fs::path folder;
    if(task.source==Source::Exe||task.source==Source::Squirrel)folder=fs::path(wide(task.target)).parent_path();
    else if(!task.probe.empty())folder=fs::path(wide(task.probe)).parent_path();
    else if(!task.directory.empty())folder=wide(task.directory);
    else {Handle process(targetProcess(task));if(process.h)folder=fs::path(processPath(process.h)).parent_path();}
    if(folder.empty()&&task.source==Source::Steam)for(auto& app:steamApps())if(app.target==task.target){folder=wide(app.directory);break;}
    std::error_code ec;if(folder.empty()||!fs::is_directory(folder,ec))throw std::runtime_error("The app's target folder is unavailable or no longer exists");return folder;
}
static void sortChoices(std::vector<AppChoice>& apps) {
    std::sort(apps.begin(),apps.end(),[](auto& a,auto& b){return _stricmp(a.name.c_str(),b.name.c_str())<0;});
    std::unordered_set<std::string> targets;
    apps.erase(std::remove_if(apps.begin(),apps.end(),[&](auto& a){return !targets.insert(std::to_string(int(a.source))+a.target).second;}),apps.end());
}
// Valve's local manifests are discovery hints. Launches use Steam's public URI protocol.
std::vector<AppChoice> steamApps() {
    wchar_t path[32768];DWORD bytes=sizeof(path);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,path,&bytes)!=ERROR_SUCCESS)return {};
    std::vector<fs::path> roots{fs::path(path)};
    std::ifstream libraries(roots[0]/L"steamapps"/L"libraryfolders.vdf");std::string text((std::istreambuf_iterator<char>(libraries)),{});
    std::regex libraryPattern("\"path\"\\s*\"([^\"]+)\"");
    for(std::sregex_iterator it(text.begin(),text.end(),libraryPattern),end;it!=end;++it) {
        std::string value=(*it)[1];size_t at=0;while((at=value.find("\\\\",at))!=std::string::npos)value.replace(at,2,"\\");roots.push_back(wide(value));
    }
    std::vector<AppChoice> apps;
    for(auto& root:roots) {
        std::error_code ec;fs::directory_iterator files(root/L"steamapps",ec);if(ec)continue;
        for(auto& entry:files) {
            auto name=entry.path().filename().wstring();if(name.rfind(L"appmanifest_",0)!=0||entry.path().extension()!=L".acf")continue;
            std::ifstream file(entry.path());std::string content((std::istreambuf_iterator<char>(file)),{});
            auto field=[&](const std::string& key) {std::smatch match;return std::regex_search(content,match,std::regex("\""+key+"\"\\s*\"([^\"]*)\""))?match[1].str():std::string{};};
            auto id=field("appid"),title=field("name"),install=field("installdir");
            if(id.empty()||title.empty())continue;
            AppChoice app;app.name=title;app.source=Source::Steam;app.target=id;app.directory=utf8((root/L"steamapps"/L"common"/wide(install)).wstring());app.description="Steam · "+id;apps.push_back(app);
        }
    }sortChoices(apps);return apps;
}
struct ChildProcess { DWORD host=0,target=0; };
static BOOL CALLBACK collectChildProcess(HWND window,LPARAM parameter) {
    auto& child=*reinterpret_cast<ChildProcess*>(parameter);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid!=child.host)child.target=pid;return TRUE;
}
static BOOL CALLBACK collectRunningApp(HWND window,LPARAM parameter) {
        if(!eligibleWindow(window))return TRUE;DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==GetCurrentProcessId())return TRUE;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));if(!process.h)return TRUE;
        auto path=processPath(process.h);if(path.empty())return TRUE;
        // UWP frame hosts own a shell window; resolve the actual app's child process.
        if(_wcsicmp(fs::path(path).filename().c_str(),L"ApplicationFrameHost.exe")==0) {
            ChildProcess child{pid};
            EnumChildWindows(window,collectChildProcess,reinterpret_cast<LPARAM>(&child));
            if(!child.target)return TRUE;pid=child.target;process=Handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));if(!process.h)return TRUE;path=processPath(process.h);
        }
        wchar_t title[1024];GetWindowTextW(window,title,1024);AppChoice app;app.name=utf8(title);app.target=utf8(path);app.description=utf8(fs::path(path).filename().wstring());app.directory=utf8(fs::path(path).parent_path().wstring());
        auto id=appId(pid);if(!id.empty()){app.source=Source::Installed;app.target=id;app.probe=utf8(path);app.description="Windows app · "+app.description;}
        reinterpret_cast<std::vector<AppChoice>*>(parameter)->push_back(app);return TRUE;
}
std::vector<AppChoice> runningApps() {
    std::vector<AppChoice> apps;
    EnumWindows(collectRunningApp,reinterpret_cast<LPARAM>(&apps));
    auto steam=steamApps();
    for(auto& app:apps)if(app.source==Source::Exe) {
        auto path=identityPath(wide(app.target));
        for(auto& candidate:steam) {
            auto folder=identityPath(wide(candidate.directory))+L"\\";
            if(path.rfind(folder,0)==0){app.probe=app.target;app.target=candidate.target;app.source=Source::Steam;app.directory=candidate.directory;app.description="Steam · "+candidate.name;app.name=candidate.name;break;}
        }
    }for(auto& app:apps)normalizeChoice(app);sortChoices(apps);return apps;
}
std::vector<AppChoice> installedApps() {
    std::vector<AppChoice> apps;ComPtr<IShellItem> folder;
    if(SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder,KF_FLAG_DEFAULT,nullptr,IID_PPV_ARGS(&folder)))) {
        ComPtr<IEnumShellItems> enumeration;
        if(SUCCEEDED(folder->BindToHandler(nullptr,BHID_EnumItems,IID_PPV_ARGS(&enumeration)))) {
            ComPtr<IShellItem> item;
            while(enumeration->Next(1,&item,nullptr)==S_OK) {
                PWSTR display=nullptr;ComPtr<IShellItem2> item2;item.As(&item2);
                if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&display))&&item2) {
                    PWSTR id=nullptr,target=nullptr;AppChoice app;app.name=utf8(display);app.description="Windows app";
                    item2->GetString(PKEY_AppUserModel_ID,&id);item2->GetString(PKEY_Link_TargetParsingPath,&target);
                    if(id&&wcschr(id,L'!')){app.source=Source::Installed;app.target=utf8(id);}
                    else if(target&&fs::path(target).extension()==L".exe"){app.target=utf8(target);app.directory=utf8(fs::path(target).parent_path().wstring());}
                    if(!app.target.empty())apps.push_back(app);
                    CoTaskMemFree(id);CoTaskMemFree(target);
                }CoTaskMemFree(display);item.Reset();
            }
        }
    }
    for(const auto& known:{FOLDERID_StartMenu,FOLDERID_CommonStartMenu}) {
        PWSTR start=nullptr;if(FAILED(SHGetKnownFolderPath(known,0,nullptr,&start)))continue;fs::path root(start);CoTaskMemFree(start);
        std::error_code ec;fs::recursive_directory_iterator entries(root,fs::directory_options::skip_permission_denied,ec);if(ec)continue;
        for(auto& entry:entries) {
            if(entry.path().extension()!=L".lnk")continue;ComPtr<IShellLinkW> link;ComPtr<IPersistFile> persist;
            if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))))continue;link.As(&persist);
            if(FAILED(persist->Load(entry.path().c_str(),STGM_READ)))continue;
            wchar_t target[32768]={},args[32768]={};link->GetPath(target,32768,nullptr,SLGP_RAWPATH);link->GetArguments(args,32768);
            // Only direct launch entries; administrative/uninstall links are not offered.
            auto stem=entry.path().stem().wstring(),lower=identityPath(target);
            if(fs::path(lower).extension()!=L".exe"||lower.find(L"unins")!=std::wstring::npos)continue;
            AppChoice app;app.name=utf8(stem);app.target=utf8(target);app.directory=utf8(fs::path(target).parent_path().wstring());app.description="Start Menu";
            if(*args) {
                int argc=0;auto parsed=CommandLineToArgvW((std::wstring(L"launcher ")+args).c_str(),&argc);
                if(parsed&&argc==3&&_wcsicmp(fs::path(target).filename().c_str(),L"Update.exe")==0&&std::wstring(parsed[1])==L"--processStart"&&fs::path(parsed[2]).filename()==fs::path(parsed[2])&&fs::path(parsed[2]).extension()==L".exe") {
                    app.source=Source::Squirrel;app.application=utf8(parsed[2]);app.arguments=utf8(args);app.description="App launcher";
                }else {if(parsed)LocalFree(parsed);continue;}
                LocalFree(parsed);
            }
            normalizeChoice(app);apps.push_back(app);
        }
    }for(auto& app:apps)normalizeChoice(app);sortChoices(apps);return apps;
}
struct WindowsBackend::Impl {
    struct Record {Task task;Handle process,launcher,job;bool managed=false,reused=false;};
    std::vector<Record> records;
};
WindowsBackend::WindowsBackend():impl_(std::make_unique<Impl>()){}
WindowsBackend::~WindowsBackend()=default;
Launch WindowsBackend::start(const Task& original) {
    Task task=original;normalizeLaunchTarget(task);
    Impl::Record record;record.task=task;Launch result;
    if(task.source==Source::Exe||task.source==Source::Squirrel) {
        auto path=wide(task.target);
        if(!fs::exists(path))return {-1,false,false,"Executable not found. Edit the app to choose its location."};
        record.process=Handle(targetProcess(task));
        if(record.process.h){record.reused=true;}
        else {
            record.job=Handle(CreateJobObjectW(nullptr,nullptr));if(!record.job.h)return {-1,false,false,systemError()};
            std::wstring command=quote(path);if(!task.arguments.empty())command+=L" "+wide(task.arguments);
            auto directory=task.directory.empty()?fs::path(path).parent_path().wstring():wide(task.directory);
            STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
            if(!CreateProcessW(path.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,directory.empty()?nullptr:directory.c_str(),&startup,&info))return {-1,false,false,systemError()};
            Handle thread(info.hThread);record.process=Handle(info.hProcess);
            if(!AssignProcessToJobObject(record.job.h,record.process.h)){auto error=systemError();TerminateProcess(record.process.h,1);return {-1,false,false,"Cannot track process safely: "+error};}
            if(ResumeThread(thread.h)==DWORD(-1)){auto error=systemError();TerminateJobObject(record.job.h,1);return {-1,false,false,error};}
            record.managed=true;
            if(task.source==Source::Squirrel)record.launcher=std::move(record.process);
        }
    } else if(task.source==Source::Steam) {
        record.process=Handle(targetProcess(task));
        if(record.process.h)record.reused=true;
        else {
            if(task.target.empty()||task.target.find_first_not_of("0123456789")!=std::string::npos)return {-1,false,false,"Invalid Steam App ID"};
            auto uri=L"steam://run/"+wide(task.target);
            SHELLEXECUTEINFOW launch{sizeof(launch)};launch.fMask=SEE_MASK_FLAG_NO_UI;launch.lpVerb=L"open";launch.lpFile=uri.c_str();launch.nShow=SW_SHOWNORMAL;
            if(!ShellExecuteExW(&launch))return {-1,false,false,"Steam could not launch: "+systemError()};
        }
    } else {
        // AUMID activation returns the actual instance. Brokered instances are observed, never force-owned.
        record.process=Handle(targetProcess(task));
        if(record.process.h)record.reused=true;
        else {
            ComPtr<IApplicationActivationManager> manager;
            HRESULT hr=CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&manager));DWORD pid=0;
            if(SUCCEEDED(hr)){auto id=wide(task.target),args=wide(task.arguments);hr=manager->ActivateApplication(id.c_str(),args.c_str(),AO_NONE,&pid);}
            if(FAILED(hr))return {-1,false,false,"Windows app activation failed: "+systemError(DWORD(hr))};
            record.process=Handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        }
    }
    result.ticket=int(impl_->records.size());result.managed=record.managed;result.reused=record.reused;
    impl_->records.push_back(std::move(record));return result;
}
Observation WindowsBackend::poll(int ticket) {
    auto& record=impl_->records.at(ticket);
    if(!record.process.h&&record.task.source!=Source::Exe)record.process=Handle(targetProcess(record.task));
    if(!record.process.h) {
        if(record.launcher.h&&WaitForSingleObject(record.launcher.h,0)==WAIT_OBJECT_0){DWORD code=0;GetExitCodeProcess(record.launcher.h,&code);if(code)return {true,false,false,true,int(code)};}
        return {};
    }
    Observation state;state.known=true;state.alive=WaitForSingleObject(record.process.h,0)==WAIT_TIMEOUT;
    if(!state.alive){DWORD exitCode=0;GetExitCodeProcess(record.process.h,&exitCode);state.exited=true;state.exitCode=int(exitCode);return state;}
    struct WindowCheck {DWORD pid;bool found=false;} check{GetProcessId(record.process.h)};
    EnumWindows([](HWND window,LPARAM p)->BOOL {auto& check=*reinterpret_cast<WindowCheck*>(p);DWORD pid;GetWindowThreadProcessId(window,&pid);if(pid==check.pid&&IsWindowVisible(window)){DWORD_PTR answer;check.found=SendMessageTimeoutW(window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,30,&answer)!=0;if(check.found)return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&check));
    state.window=check.found;return state;
}
bool WindowsBackend::ownedAlive(int ticket) {
    auto& r=impl_->records.at(ticket);if(!r.managed)return false;
    if(r.job.h){JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};if(QueryInformationJobObject(r.job.h,JobObjectBasicAccountingInformation,&info,sizeof(info),nullptr))return info.ActiveProcesses>0;}
    return r.process.h&&WaitForSingleObject(r.process.h,0)==WAIT_TIMEOUT;
}
std::string WindowsBackend::close(int ticket,bool force) {
    auto& r=impl_->records.at(ticket);if(force&&!r.managed)return "This app was not directly launched by this session";
    if(force){if(r.job.h?TerminateJobObject(r.job.h,1):TerminateProcess(r.process.h,1))return {};return systemError();}
    std::unordered_set<DWORD> pids;
    if(r.job.h) {
        std::vector<unsigned char> buffer(sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)+sizeof(ULONG_PTR)*1024);
        auto list=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());
        if(QueryInformationJobObject(r.job.h,JobObjectBasicProcessIdList,list,DWORD(buffer.size()),nullptr))for(DWORD i=0;i<list->NumberOfProcessIdsInList;++i)pids.insert(DWORD(list->ProcessIdList[i]));
    }
    if(r.process.h)pids.insert(GetProcessId(r.process.h));
    struct Context {const std::unordered_set<DWORD>* pids;int sent=0;} context{&pids};
    EnumWindows([](HWND window,LPARAM p)->BOOL {auto& c=*reinterpret_cast<Context*>(p);DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(c.pids->contains(pid)&&PostMessageW(window,WM_CLOSE,0,0))++c.sent;return TRUE;},reinterpret_cast<LPARAM>(&context));
    return context.sent?std::string{}:"No closeable window; use the app's Exit or Windows Task Manager";
}
}
