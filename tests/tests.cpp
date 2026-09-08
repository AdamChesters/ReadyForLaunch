#include "engine.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

using namespace rfl;
int checks=0;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
Task task(std::string id,Timing timing=Timing::Started) {Task t;t.id=id;t.name=id;t.target="fixture.exe";t.timing=timing;t.settleMs=0;return t;}
struct Fake final:Backend {
    std::vector<std::string> started,closed;std::vector<Observation> states;std::vector<bool> owned;
    bool reuse=false,launchError=false,throwOnLaunch=false;
    Launch start(const Task& t)override {
        if(throwOnLaunch)throw std::runtime_error("Launch exception");
        started.push_back(t.id);if(launchError)return {-1,false,false,"Launch failed"};
        states.push_back({true,true,true,false,0});owned.push_back(!reuse);return {int(states.size()-1),reuse,!reuse,{}};
    }
    Observation poll(int t)override{return states.at(t);}
    std::string close(int t,bool force)override{closed.push_back(started.at(t)+(force?"!":""));states.at(t).alive=false;states.at(t).exited=true;return {};}
    bool ownedAlive(int t)override{return owned.at(t)&&states.at(t).alive;}
};
Profile profile() {return {"p","Test",false,{{"a","A",{}, {task("a1"),task("a2")}},{"b","B","a",{task("b1"),task("b2",Timing::Together)}},{"c","C","b",{task("c1")}}}};}
int main() {
    try {
        {auto p=profile();Fake f;Engine e(f);check(e.begin(p).empty(),"valid linear profile");e.tick(0);check(f.started.size()==1,"first group first app only");e.tick(1);check(f.started.size()==2,"second app starts after first");e.tick(2);check(f.started.size()==4,"group B starts together after all A");e.tick(3);check(f.started.size()==5,"third group follows second");e.stop(false,10);check(f.closed.front()=="c1","stop consumers first");for(int i=11;i<20;++i)e.tick(i);check(!e.active(),"graceful session finishes");}
        {auto p=profile();p.groups[0].tasks[0].enabled=false;Fake f;Engine e(f);check(e.begin(p).empty(),"disable first task");e.tick(0);check(f.started.front()=="a2","next first keeps group role");e.tick(1);check(f.started.size()==3,"retains dependent group gate");}
        {auto p=profile();p.groups[1].tasks[0].enabled=false;Fake f;Engine e(f);e.begin(p);e.tick(0);check(f.started.size()==1,"disabling dependent first cannot bypass gate");}
        {auto p=profile();p.groups[0].afterGroup="c";check(!validate(p).empty(),"cycles rejected");check(!canFollow(p,"a","c"),"picker rejects cycle");}
        {auto p=profile();p.groups[0].tasks.clear();check(!validate(p).empty(),"empty dependency rejected");}
        {auto p=profile();p.groups[0].tasks[1].timing=Timing::Together;p.groups[0].tasks[1].settleMs=500;Fake f;Engine e(f);e.begin(p);e.tick(0);check(f.started.size()==2,"parallel first group");e.tick(1);e.tick(100);check(f.started.size()==2,"whole group waits slowest member");e.tick(502);check(f.started.size()==4,"dependent group released after settle");}
        {auto p=profile();p.groups[2].afterGroup="a";Fake f;Engine e(f);e.begin(p);for(int i=0;i<5;++i)e.tick(i);check(f.started.size()==5,"two groups share predecessor");}
        {auto p=profile();p.groups[0].tasks[1].timing=Timing::Delay;p.groups[0].tasks[1].delayMs=1000;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(999);check(f.started.size()==1,"delay never early");e.tick(1000);check(f.started.size()==2,"delay measured from dispatch");}
        {auto p=profile();p.groups[0].tasks[0].readiness=Readiness::Completion;p.groups[0].tasks[1].timing=Timing::Finished;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(1);check(f.started.size()==1,"helper must exit");f.states[0]={true,false,false,true,0};e.tick(2);check(f.started.size()==2,"successful completion gate");}
        {auto p=profile();Fake f;Engine e(f);e.begin(p);e.tick(0);f.states[0]={true,false,false,true,7};e.tick(1);e.tick(2);check(f.started.size()==1,"failure blocks descendants");check(e.retry("a1"),"failed exited app can retry");e.tick(3);check(f.started.size()==2,"retry dispatches again");}
        {auto p=profile();p.groups[0].tasks[0].readiness=Readiness::Manual;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(1);check(f.started.size()==1,"manual gate holds");e.confirm("a1");e.tick(2);check(f.started.size()==2,"manual confirmation releases");}
        {auto p=profile();p.groups[0].tasks[0].readiness=Readiness::Manual;p.groups[0].tasks[0].timeoutMs=1000;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(1000);check(e.nodes()[0].phase==Phase::Failed,"timeout reported");check(!e.retry("a1"),"do not duplicate living failed app");}
        {auto p=profile();Fake f;Engine e(f);e.begin(p);e.tick(0);e.stop(true,1);e.tick(2000);check(f.started.size()==1,"stop cancels queued launch");check(f.closed[0]=="a1!","emergency force request");}
        {auto p=profile();Fake f;f.reuse=true;Engine e(f);e.begin(p);e.tick(0);e.tick(1);e.stop(true,2);check(f.closed.empty(),"already-running instances never killed");}
        {auto original=defaults();auto copied=duplicate(profile(),"Copy");original.profiles.push_back(copied);auto reloaded=deserialize(serialize(original));check(serialize(reloaded)==serialize(original),"settings roundtrip preserves profiles");check(copied.groups[1].afterGroup==copied.groups[0].id,"copy remaps group references");check(copied.groups[0].id!="a","copy uses independent IDs");}
        {auto p=profile();p.groups[0].tasks[1].timing=Timing::Delay;p.groups[0].tasks[1].delayMs=1000;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(1);f.states[0]={true,false,false,true,0};e.tick(1000);check(f.started.size()==1,"exited persistent prerequisite cannot release delayed app");}
        {auto p=profile();Fake f;Engine e(f);e.begin(p);e.tick(0);f.reuse=true;e.tick(1);e.stop(false,2);check(f.closed.empty(),"keep owned prerequisite while reused dependent remains alive");e.stop(true,3);check(f.closed.size()==1&&f.closed[0]=="a1!","force stop only owned prerequisite");}
        {auto p=profile();Fake f;f.throwOnLaunch=true;Engine e(f);e.begin(p);e.tick(0);check(e.nodes()[0].phase==Phase::Failed,"launch exception becomes row failure");check(e.nodes()[1].phase==Phase::Waiting,"launch exception does not release dependency");}
        std::cout<<checks<<" scheduler/profile assertions passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
