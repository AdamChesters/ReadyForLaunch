#include "engine.hpp"
#include <unordered_map>
#include <functional>

namespace rfl {
const char* phaseName(Phase p) {
    static const char* names[]={"Waiting","Launching","Running","Completed","Failed","Cancelled","Stopping","Needs attention","Stopped"}; return names[int(p)];
}
std::vector<std::string> Engine::begin(const Profile& p) {
    if(active())return {"A session is already active. Stop it before GO."};
    auto errors=validate(p);if(!errors.empty())return errors;
    nodes_.clear();stopping_=false;force_=false;
    std::unordered_map<std::string,std::vector<size_t>> groups;
    for(auto& g:p.groups)for(auto& t:g.tasks)if(t.enabled) {groups[g.id].push_back(nodes_.size());Node n;n.task=t;n.group=g.id;nodes_.push_back(n);}
    for(auto& g:p.groups) {
        std::vector<Dependency> gate;
        if(!g.afterGroup.empty())for(auto i:groups.at(g.afterGroup))gate.push_back({i,false,0});
        const auto& indices=groups[g.id];
        for(size_t pos=0;pos<indices.size();++pos) {
            auto& n=nodes_[indices[pos]]; n.dependencies=gate;
            if(pos) {
                auto previous=indices[pos-1];
                if(n.task.timing==Timing::Together)n.dependencies=nodes_[previous].dependencies;
                else n.dependencies.push_back({previous,n.task.timing==Timing::Delay,n.task.timing==Timing::Delay?n.task.delayMs:0});
            }
        }
    }return {};
}
const Node* Engine::find(const std::string& id)const {for(auto& n:nodes_)if(n.task.id==id)return &n;return nullptr;}
void Engine::confirm(const std::string& id) {for(auto& n:nodes_)if(n.task.id==id&&n.phase==Phase::Launching)n.confirmed=true;}
bool Engine::retry(const std::string& id) {
    if(stopping_)return false;
    for(auto& n:nodes_)if(n.task.id==id&&n.phase==Phase::Failed) {
        if(n.launch.ticket>=0&&(backend_.ownedAlive(n.launch.ticket)||backend_.poll(n.launch.ticket).alive))return false;
        n.phase=Phase::Waiting;n.launch={};n.dispatched=false;n.ready=false;n.confirmed=false;n.detectedAt=-1;n.detail.clear();return true;
    }return false;
}
bool Engine::active() {
    for(auto& n:nodes_) {
        if(!stopping_&&(n.phase==Phase::Waiting||n.phase==Phase::Launching||n.phase==Phase::Running||n.phase==Phase::Attention))return true;
        if(n.launch.ticket>=0&&backend_.ownedAlive(n.launch.ticket))return true;
    }return false;
}
void Engine::tick(int64_t now) {
    if(stopping_){stopTick(now);return;}
    for(auto& n:nodes_) {
        if(!n.dispatched||n.phase==Phase::Failed||n.phase==Phase::Completed||n.phase==Phase::Stopped)continue;
        auto state=backend_.poll(n.launch.ticket);
        if(state.exited) {
            n.exitSuccess=state.exitCode==0;
            if(n.task.readiness==Readiness::Completion&&n.exitSuccess) {
                if(n.detectedAt<0)n.detectedAt=now;
                if(now-n.detectedAt>=n.task.settleMs){n.phase=Phase::Completed;n.ready=true;n.detail="Finished successfully";}
            } else if(n.task.readiness==Readiness::Completion||!n.ready||state.exitCode!=0) {
                n.phase=Phase::Failed;n.ready=false;n.detail="Exited with code "+std::to_string(state.exitCode)+(state.exitCode==0?" before startup was confirmed":"");
            } else {n.phase=Phase::Completed;n.ready=false;n.detail="Exited";}
        } else {
            bool detected=state.known&&state.alive;
            if(n.task.readiness==Readiness::Window)detected=detected&&state.window;
            if(n.task.readiness==Readiness::Manual)detected=n.confirmed;
            if(n.task.readiness==Readiness::Completion)detected=false;
            if(detected){if(n.detectedAt<0)n.detectedAt=now;}
            else n.detectedAt=-1;
            if(detected&&now-n.detectedAt>=n.task.settleMs) {
                n.ready=true;n.phase=Phase::Running;
                n.detail=n.launch.reused?"Already running · left open on Stop":n.launch.managed?"Started by this session":"Launch only · stop unavailable";
            }
        }
        if(!n.ready&&n.phase!=Phase::Failed&&now-n.dispatchedAt>=n.task.timeoutMs){n.phase=Phase::Failed;n.detail="Startup timed out; check the app before retrying";}
    }
    // Evaluate all gates before dispatching: Together tasks share the same release tick.
    std::vector<size_t> eligible;
    for(size_t i=0;i<nodes_.size();++i) {
        auto& n=nodes_[i];if(n.phase!=Phase::Waiting)continue;
        bool ready=true;n.detail.clear();
        for(auto d:n.dependencies) {
            const auto& prior=nodes_[d.node];
            bool failed=prior.phase==Phase::Failed||prior.phase==Phase::Cancelled||(prior.phase==Phase::Completed&&!prior.ready);
            bool satisfied=d.dispatch?prior.dispatched&&now-prior.dispatchedAt>=d.delayMs:prior.ready;
            if(failed||!satisfied){ready=false;n.detail=(failed?"Blocked by ":"Waiting for ")+prior.task.name;break;}
        }
        if(ready)eligible.push_back(i);
    }
    for(auto i:eligible) {
        auto& n=nodes_[i];
        try {n.launch=backend_.start(n.task);}catch(const std::exception& error){n.launch.error=error.what();}
        if(n.launch.ticket<0&&n.launch.error.empty())n.launch.error="No launch handle was returned";
        if(!n.launch.error.empty()){n.phase=Phase::Failed;n.detail=n.launch.error;continue;}
        n.dispatched=true;n.dispatchedAt=now;n.phase=Phase::Launching;
        n.detail=n.task.readiness==Readiness::Manual?"Check the app, then Confirm ready":"Waiting for startup";
    }
}
bool Engine::hasLiveDependent(size_t index) {
    std::vector<bool> seen(nodes_.size(),false);
    std::function<bool(size_t)> visit=[&](size_t root) {
        if(seen[root])return false;seen[root]=true;
        for(size_t i=0;i<nodes_.size();++i)for(auto d:nodes_[i].dependencies)if(d.node==root) {
            if(nodes_[i].launch.ticket>=0) {
                if(backend_.ownedAlive(nodes_[i].launch.ticket))return true;
                if(!nodes_[i].launch.managed) {
                    auto state=backend_.poll(nodes_[i].launch.ticket);
                    if(state.alive||!state.known)return true;
                }
            }
            if(visit(i))return true;
        }return false;
    };return visit(index);
}
void Engine::stop(bool force,int64_t now) {
    stopping_=true;force_=force_||force;
    for(auto& n:nodes_)if(n.phase==Phase::Waiting)n.phase=Phase::Cancelled;
    if(force)for(auto& n:nodes_)if(n.launch.ticket>=0&&backend_.ownedAlive(n.launch.ticket)) {
        n.detail=backend_.close(n.launch.ticket,true);n.phase=n.detail.empty()?Phase::Stopping:Phase::Attention;n.stoppedAt=now;
    }
    stopTick(now);
}
void Engine::stopTick(int64_t now) {
    for(size_t reverse=nodes_.size();reverse>0;--reverse) {
        auto i=reverse-1;auto& n=nodes_[i];if(n.launch.ticket<0)continue;
        if(!backend_.ownedAlive(n.launch.ticket)) {
            if(n.launch.managed){n.phase=Phase::Stopped;n.detail="Stopped";}
            else {n.phase=Phase::Stopped;n.detail=n.launch.reused?"Left open · already running":"Left open · launch only";}
            continue;
        }
        if(n.phase==Phase::Stopping) {
            if(now-n.stoppedAt>=10000){n.phase=Phase::Attention;n.detail="Still running; close manually or use Emergency stop";}
            continue;
        }
        if(n.phase==Phase::Attention)continue;
        if(!force_&&hasLiveDependent(i)){n.detail="Waiting for dependent apps to exit";continue;}
        auto error=backend_.close(n.launch.ticket,force_);n.stoppedAt=now;n.phase=error.empty()?Phase::Stopping:Phase::Attention;
        n.detail=error.empty()?"Requested exit":error;
    }
}
}
