#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace rfl {
using Json = nlohmann::json;
enum class Source { Exe, Steam, Installed };
enum class Timing { Together, Started, Delay, Finished };
enum class Readiness { Process, Window, Completion, Manual };
struct Task {
    std::string id, name, target, arguments, directory, probe;
    Source source = Source::Exe;
    Timing timing = Timing::Started;
    Readiness readiness = Readiness::Process;
    bool enabled = true;
    int delayMs = 5000, timeoutMs = 60000, settleMs = 1000;
};
struct Group {
    std::string id, name, afterGroup;
    std::vector<Task> tasks;
};
struct Profile {
    std::string id, name;
    bool builtin = false;
    std::vector<Group> groups;
};
struct Settings {
    std::string selected;
    std::vector<Profile> profiles;
};
std::string newId();
Settings defaults();
Profile duplicate(const Profile& p, std::string name);
std::vector<std::string> validate(const Profile& p);
bool canFollow(const Profile& p, const std::string& group, const std::string& predecessor);
Json serialize(const Settings& s);
Settings deserialize(const Json& j);
Settings loadSettings(const std::filesystem::path& path, std::string& warning);
void saveSettings(const std::filesystem::path& path, const Settings& s);
const char* sourceName(Source s);
}
