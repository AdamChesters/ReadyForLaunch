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
    bool reuse=false,launchError=false,throwOnLaunch=false,refuse=false;
    Launch start(const Task& t)override {
        if(throwOnLaunch)throw std::runtime_error("Launch exception");
        started.push_back(t.id);if(launchError)return {-1,false,false,"Launch failed"};
        states.push_back({true,true,true,false,0});owned.push_back(!reuse);return {int(states.size()-1),reuse,!reuse,{}};
    }
    Observation poll(int t)override{return states.at(t);}
    std::string close(int t,bool force)override{closed.push_back(started.at(t)+(force?"!":""));if(!refuse||force){states.at(t).alive=false;states.at(t).exited=true;}return {};}
    bool ownedAlive(int t)override{return owned.at(t)&&states.at(t).alive;}
};
Profile profile() {return {"p","Test",false,{{"a","A",{}, {task("a1"),task("a2")}},{"b","B","a",{task("b1"),task("b2",Timing::Together)}},{"c","C","b",{task("c1")}}}};}
int main() {
    try {
        {auto p=profile();Fake f;Engine e(f);check(e.begin(p).empty(),"valid linear profile");e.tick(0);check(f.started.size()==1,"first group first app only");e.tick(1);check(f.started.size()==2,"second app starts after first");e.tick(2);check(f.started.size()==4,"group B starts together after all A");e.tick(3);check(f.started.size()==5,"third group follows second");e.stop(10);check(f.closed.front()=="c1","stop consumers first");for(int i=11;i<20;++i)e.tick(i);check(!e.active(),"graceful session finishes");}
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
        {auto p=profile();Fake f;Engine e(f);e.begin(p);e.tick(0);e.stop(1);e.tick(2000);check(f.started.size()==1,"stop cancels queued launch");check(f.closed[0]=="a1","stop requests graceful closure only");}
        {auto p=profile();Fake f;f.reuse=true;Engine e(f);e.begin(p);e.tick(0);e.tick(1);e.stop(2);check(f.closed.size()==2,"Stop asks existing listed apps to close too");check(f.closed[0]=="a2"&&f.closed[1]=="a1","existing apps receive graceful closure in dependency order");}
        {auto original=defaults();auto copied=duplicate(profile(),"Copy");original.profiles.push_back(copied);auto reloaded=deserialize(serialize(original));check(serialize(reloaded)==serialize(original),"settings roundtrip preserves profiles");check(copied.groups[1].afterGroup==copied.groups[0].id,"copy remaps group references");check(copied.groups[0].id!="a","copy uses independent IDs");}
        {auto p=profile();p.groups[0].tasks[1].timing=Timing::Delay;p.groups[0].tasks[1].delayMs=1000;Fake f;Engine e(f);e.begin(p);e.tick(0);e.tick(1);f.states[0]={true,false,false,true,0};e.tick(1000);check(f.started.size()==1,"exited persistent prerequisite cannot release delayed app");}
        {auto p=profile();Fake f;Engine e(f);e.begin(p);e.tick(0);f.reuse=true;e.tick(1);f.refuse=true;e.stop(2);check(f.closed.size()==1&&f.closed[0]=="a2","request reused dependent exit before prerequisite");e.tick(10002);check(e.nodes()[1].phase==Phase::Attention,"refusal gets attention instead of force");check(f.closed.size()==1,"never escalate refusal to forced termination");f.states[1]={true,false,false,true,0};f.refuse=false;e.tick(10003);check(f.closed.size()==2&&f.closed[1]=="a1","prerequisite closes after dependent exits");}
        {auto p=profile();Fake f;f.throwOnLaunch=true;Engine e(f);e.begin(p);e.tick(0);check(e.nodes()[0].phase==Phase::Failed,"launch exception becomes row failure");check(e.nodes()[1].phase==Phase::Waiting,"launch exception does not release dependency");}
        {auto p=profile();p.groups[1].tasks[1].timing=Timing::Go;Fake f;Engine e(f);e.begin(p);e.tick(0);check(f.started==std::vector<std::string>({"a1","b2"}),"On GO on a later row bypasses predecessor and group waits");}
        {auto p=profile();p.groups[0].tasks[1].timing=Timing::Go;p.groups[0].tasks.push_back(task("a3",Timing::Together));Fake f;Engine e(f);e.begin(p);e.tick(0);check(f.started.size()==3,"With previous shares the preceding On GO gate");}
        {auto p=profile();p.groups[1].tasks[0].enabled=false;p.groups[1].tasks[1].timing=Timing::Go;Fake f;Engine e(f);e.begin(p);e.tick(0);check(f.started.size()==1,"first enabled row retains the group's explicit dependency");}
        {auto source=profile();Profile destination{"keep-id","Keep name",true,{}};copyConfiguration(destination,source);check(destination.id=="keep-id"&&destination.name=="Keep name"&&destination.builtin,"copy preserves destination tab identity");check(destination.groups[1].afterGroup==destination.groups[0].id,"copy rewrites dependencies");check(destination.groups[0].tasks[0].id!=source.groups[0].tasks[0].id,"copied apps have independent IDs");}
        {auto settings=defaults();auto a=duplicate(profile(),"Same name"),b=duplicate(profile(),"Same name");settings.profiles.push_back(a);settings.profiles.push_back(b);auto loaded=deserialize(serialize(settings));check(loaded.profiles[5].name==loaded.profiles[6].name&&loaded.profiles[5].id!=loaded.profiles[6].id,"duplicate display names retain distinct profile IDs");}
        {auto old=serialize(defaults());old["schemaVersion"]=1;auto migrated=deserialize(old);check(serialize(migrated)["schemaVersion"]==2,"first preview profiles migrate to schema 2");old["schemaVersion"]=99;bool rejected=false;try{deserialize(old);}catch(const std::exception&){rejected=true;}check(rejected,"unknown future settings versions are rejected");}
        std::cout<<checks<<" scheduler/profile assertions passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
