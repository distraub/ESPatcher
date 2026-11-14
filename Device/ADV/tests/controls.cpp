#include "../src/ControlState.hpp"
#include <cassert>
#include <vector>
#include <iostream>
int main(){
    adv::Controls controls;adv::Input input;std::vector<adv::Event> events;
    auto emit=[&](adv::Event e){events.push_back(e);};
    input.notes[0]=true;controls.scan(input,0,emit);assert(events.size()==1&&events[0].index==60&&events[0].value==100);
    events.clear();input.notes[0]=false;input.notes[2]=true;controls.scan(input,2,emit);
    assert(events.size()==2&&events[0].index==60&&events[0].value==0&&events[1].index==62&&events[1].value==100);
    events.clear();input.octaveUp=true;controls.scan(input,4,emit);assert(controls.octave==5&&events.back().kind==adv::Kind::Panic);
    events.clear();input.octaveUp=false;controls.scan(input,6,emit);assert(events.empty()); // Held key stays blocked.
    input.notes[2]=false;controls.scan(input,8,emit);input.notes[2]=true;controls.scan(input,10,emit);assert(events.back().index==74);
    events.clear();input.bell=true;controls.scan(input,12,emit);assert(controls.instrument==1&&events.back().kind==adv::Kind::Instrument&&events.back().index==1);
    input.bell=false;input.panic=true;controls.scan(input,14,emit);input.panic=false;events.clear();controls.scan(input,16,emit);assert(events.empty());
    input.notes.fill(false);input.plus=true;controls.scan(input,100,emit);assert(controls.parameters[4]>.2f);
    auto before=controls.parameters[4];controls.scan(input,499,emit);assert(controls.parameters[4]==before);controls.scan(input,500,emit);assert(controls.parameters[4]>before);
    for(uint32_t t=600;t<5000;t+=100)controls.scan(input,t,emit);assert(controls.parameters[4]==.5f);
    input.plus=false;input.next=true;controls.scan(input,5001,emit);assert(controls.selected==6);
    for(int n=0;n<20;n++){input.octaveUp=true;controls.scan(input,6000+n*2,emit);input.octaveUp=false;controls.scan(input,6001+n*2,emit);}assert(controls.octave==7);
    for(int n=0;n<20;n++){input.octaveDown=true;controls.scan(input,7000+n*2,emit);input.octaveDown=false;controls.scan(input,7001+n*2,emit);}assert(controls.octave==1);
    input=adv::Input{};events.clear();input.testChord=true;controls.scan(input,8000,emit);
    assert(controls.testChord&&controls.diagnostics&&events.size()==5);
    events.clear();input.testChord=false;controls.scan(input,8001,emit);assert(events.empty());
    controls.scan(input,9000,emit);assert(events.size()==4);
    events.clear();input.panic=true;controls.scan(input,9001,emit);assert(!controls.testChord&&events.back().kind==adv::Kind::Panic);
    input=adv::Input{};controls.scan(input,9002,emit);events.clear();
    int initialTheme=controls.theme;input.themeNext=true;controls.scan(input,9003,emit);assert(controls.theme==(initialTheme+1)%GS_THEME_COUNT&&events.empty());
    input.themeNext=false;input.themePrevious=true;controls.scan(input,9004,emit);assert(controls.theme==initialTheme&&events.empty());
    std::cout<<"PASS: equal-count key swaps, note releases, octave/panic held-key suppression, instrument switch, repeat timing, parameter and octave bounds, four-note stress chord, theme changes without audio events\n";
}
