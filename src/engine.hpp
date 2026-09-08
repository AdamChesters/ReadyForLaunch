#pragma once
#include "model.hpp"
#include <memory>

namespace rfl {
struct Launch { int ticket=-1; bool reused=false, managed=false; std::string error; };
struct Observation { bool known=false, alive=false, window=false, exited=false; int exitCode=0; };
class Backend {
public:
    virtual ~Backend()=default;
    virtual Launch start(const Task& task)=0;
    virtual Observation poll(int ticket)=0;
    virtual std::string close(int ticket,bool force)=0;
    virtual bool ownedAlive(int ticket)=0;
};
enum class Phase { Waiting, Launching, Running, Completed, Failed, Cancelled, Stopping, Attention, Stopped };
struct Dependency { size_t node; bool dispatch=false; int delayMs=0; };
struct Node {
    Task task; std::string group;
    std::vector<Dependency> dependencies;
    Phase phase=Phase::Waiting; Launch launch;
    bool dispatched=false, ready=false, confirmed=false, exitSuccess=false, alive=false;
    int64_t dispatchedAt=0, detectedAt=-1, stoppedAt=0;
    std::string detail;
};
class Engine {
    Backend& backend_;
    bool stopping_=false;
    std::vector<Node> nodes_;
    void stopTick(int64_t now);
    bool hasLiveDependent(size_t index);
public:
    explicit Engine(Backend& backend):backend_(backend){}
    std::vector<std::string> begin(const Profile& profile);
    void tick(int64_t now);
    void stop(int64_t now);
    void confirm(const std::string& id);
    bool retry(const std::string& id);
    bool active();
    const std::vector<Node>& nodes()const{return nodes_;}
    const Node* find(const std::string& id)const;
    bool stopping()const{return stopping_;}
};
const char* phaseName(Phase phase);
}
