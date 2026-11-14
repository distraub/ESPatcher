#include "../Core/Navigation.hpp"
#include "../Device/ADV/src/ControlState.hpp"
#include <cassert>
#include <vector>
#include <iostream>
int main(){
    gs::Navigation nav;gs::NavInput input;uint32_t time=0;
    auto scan=[&]{time+=20;nav.scan(input,time);};
    input.pageKey=1;scan();assert(nav.page==gs::Page::Global);
    input.pageKey=-1;scan();
    input.fn=true;input.numbers=4;scan();assert(nav.page==gs::Page::OSC);
    input.fn=false;scan();assert(nav.selected()==0); // Fn shortcut remains suppressed until number release.
    input.numbers=0;scan();input.numbers=8;scan();assert(nav.selected()==3&&nav.address()==3);
    input.right=true;scan();assert(nav.editAddress==3&&nav.delta==.02f);
    scan();assert(nav.editAddress==-1); // Repeat delay.
    time+=400;scan();assert(nav.editAddress==3);
    time+=2100;scan();assert(nav.delta==.05f);
    input.right=false;scan();input.fine=true;input.right=true;scan();assert(nav.delta==.002f);
    input={};scan();nav.go(gs::Page::LFO);assert(nav.address()==18);
    input.tab=true;scan();assert(nav.editing&&nav.selected()==1);input.tab=false;scan();input.right=true;scan();assert(nav.selected()==2);
    input.right=false;scan();input.up=true;scan();assert(nav.editAddress==7&&nav.delta>0);
    input={};scan();nav.go(gs::Page::OSC);assert(nav.selected()==3); // Per-page memory.
    nav.go(gs::Page::Global);input.down=true;scan();assert(nav.menu==1);input={};scan();input.enter=true;scan();assert(nav.section==2);
    input={};scan();input.down=true;scan();assert(nav.theme==25&&!nav.saveTheme);input={};scan();input.escape=true;scan();assert(nav.theme==24&&nav.section==0);
    input={};scan();input.enter=true;scan();input={};scan();input.down=true;scan();input={};scan();input.enter=true;scan();assert(nav.saveTheme&&nav.savedTheme==25);
    input={};scan();assert(!nav.saveTheme);input.enter=true;scan();input={};scan();input.down=true;scan();nav.go(gs::Page::Home);assert(nav.theme==25); // Leaving preview cancels.
    nav.go(gs::Page::Global);nav.menu=0;input={};scan();input.enter=true;scan();input={};scan();input.down=true;scan();assert(nav.loadInstrument==-1);input={};scan();input.enter=true;scan();assert(nav.loadInstrument==1&&nav.page==gs::Page::Home);
    nav.go(gs::Page::Global);nav.menu=2;input={};scan();input.enter=true;scan();assert(nav.section==3);
    input={};scan();input.up=true;scan();assert(nav.editAddress==14&&std::abs(gs::editParameter(14,120,nav.delta)-121)<.001f);
    input={};scan();input.up=true;input.fine=true;scan();assert(std::abs(gs::editParameter(14,120,nav.delta)-120.1f)<.001f);
    assert(gs::Navigation::binding(gs::Page::LFO,4)==-1);
    nav.go(gs::Page::Arp);assert(nav.address()==9&&gs::Navigation::binding(gs::Page::Arp,1)==15&&gs::Navigation::binding(gs::Page::Arp,5)==8);
    assert(gs::editParameter(0,1,.002f)==2);
    assert(gs::editParameter(3,1000,.002f)>1000&&gs::editParameter(3,1000,.002f)<gs::editParameter(3,1000,.02f));
    adv::Controls controls;adv::Input device;std::vector<adv::Event> events;auto emit=[&](adv::Event e){events.push_back(e);};
    device.notes[0]=true;controls.scan(device,0,emit);events.clear();
    device.navigation.fn=true;device.navigation.numbers=2;controls.scan(device,20,emit);assert(events.empty()&&controls.nav.page==gs::Page::Global&&controls.midi[0]==60);
    device.navigation={};controls.scan(device,40,emit);device.navigation.numbers=1;device.navigation.right=true;controls.scan(device,60,emit);
    assert(events.size()==1&&events[0].kind==adv::Kind::Parameter&&events[0].index==3&&controls.parameters[3]>5000);
    device.play=true;device.navigation={};events.clear();controls.scan(device,70,emit);assert(controls.parameters[8]==1&&events.size()==1&&events[0].index==8);
    events.clear();controls.scan(device,75,emit);assert(events.empty());
    device.play=false;controls.scan(device,76,emit);device.play=true;controls.scan(device,77,emit);assert(controls.parameters[8]==0);
    device.notes[0]=false;device.navigation={};controls.scan(device,80,emit);assert(events.back().kind==adv::Kind::Note&&events.back().value==0);
    std::cout<<"PASS: navigation pages, Fn suppression, page memory, held/latched virtual knobs, fine/repeat acceleration, theme preview/cancel/save, explicit instrument load, playing across screens\n";
}
