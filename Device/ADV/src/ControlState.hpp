#pragma once
#include <array>
#include "Themes.h"
#include "../../../Core/Navigation.hpp"
#include <algorithm>
#include <cstdint>

namespace adv {
constexpr unsigned sampleRate=32000, blockFrames=256;
constexpr char noteKeys[]="awsedftgyhujk";
enum class Kind:uint8_t { Note, Parameter, Panic, Instrument };
struct Event { Kind kind; uint8_t index; float value; };
struct Input {
    gs::NavInput navigation{};
    std::array<bool,13> notes{};
    bool play=false;
    bool octaveDown=false,octaveUp=false,analog=false,bell=false;
    bool themeNext=false,themePrevious=false;
    bool next=false,minus=false,plus=false,panic=false,diagnostics=false,testChord=false;
};
struct Controls {
    gs::Navigation nav;
    int octave=4,instrument=0,selected=4,theme=GS_DEFAULT_THEME;
    bool diagnostics=false,testChord=false;
    std::array<float,19> parameters{{1,.015f,.25f,5000,.2f,0,1,0,0,0,4,.5f,1,0,120,3,1,1,1}};
    std::array<int,13> midi{};
    std::array<bool,13> blocked{};
    Input previous{};
    uint32_t repeatAt=0,nextTestChord=0;
    Controls(){midi.fill(-1);}
    template<class Sink> void silence(const Input& input,Sink&& emit){
        emit(Event{Kind::Panic,0,0});midi.fill(-1);blocked=input.notes;testChord=false;
    }
    template<class Sink> void scan(const Input& input,uint32_t ms,Sink&& emit){
        for(unsigned i=0;i<13;i++){
            if(!input.notes[i]){
                blocked[i]=false;
                if(midi[i]>=0){emit(Event{Kind::Note,uint8_t(midi[i]),0});midi[i]=-1;}
            }
        }
        bool reset=false;
        if(input.play&&!previous.play){parameters[8]=parameters[8]>=.5f?0:1;emit(Event{Kind::Parameter,8,parameters[8]});}
        int previousTheme=nav.theme;auto previousPage=nav.page;nav.scan(input.navigation,ms);
        if(nav.page!=previousPage)diagnostics=false;
        if(nav.theme!=previousTheme)theme=nav.theme;
        if(nav.editAddress>=0){
            unsigned address=unsigned(nav.editAddress);
            if(address==17&&parameters[16]<.5f)address=6;
            parameters[address]=gs::editParameter(address,parameters[address],nav.delta,sampleRate*.45f);
            selected=int(address);emit(Event{Kind::Parameter,uint8_t(address),parameters[address]});
        }
        if(nav.loadInstrument>=0){
            instrument=nav.loadInstrument;silence(input,emit);emit(Event{Kind::Instrument,uint8_t(instrument),0});reset=true;
        }
        if(input.panic&&!previous.panic){silence(input,emit);reset=true;}
        if((input.analog&&!previous.analog)||(input.bell&&!previous.bell)){
            instrument=input.bell?1:0;silence(input,emit);emit(Event{Kind::Instrument,uint8_t(instrument),0});reset=true;
        }
        if((input.octaveDown&&!previous.octaveDown)||(input.octaveUp&&!previous.octaveUp)){
            octave=std::clamp(octave+(input.octaveUp?1:-1),1,7);silence(input,emit);reset=true;
        }
        if(input.next&&!previous.next)selected=selected==3?5:selected==5?0:selected==2?4:selected==4?6:selected==6?7:selected==7?3:selected+1;
        if(input.themeNext&&!previous.themeNext)theme=(theme+1)%GS_THEME_COUNT;
        if(input.themePrevious&&!previous.themePrevious)theme=(theme+GS_THEME_COUNT-1)%GS_THEME_COUNT;
        if(input.diagnostics&&!previous.diagnostics){diagnostics=!diagnostics;if(diagnostics)nav.go(gs::Page::Home);}
        bool edge=(input.minus&&!previous.minus)||(input.plus&&!previous.plus);
        bool repeat=(input.minus||input.plus)&&int32_t(ms-repeatAt)>=0;
        if(edge||repeat){
            int direction=input.plus?1:-1;
            static const float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,sampleRate*.45f,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
            auto& v=parameters[selected];
            if(selected==0)v+=direction;
            else if(selected==4||selected==5||selected==7)v+=direction*.025f;
            else v*=direction>0?1.2f:1.f/1.2f;
            v=std::clamp(v,lo[selected],hi[selected]);emit(Event{Kind::Parameter,uint8_t(selected),v});
            repeatAt=ms+(edge?400:100);
        }
        if(input.testChord&&!previous.testChord){
            bool enable=!testChord;silence(input,emit);testChord=enable;diagnostics=true;reset=true;
            nextTestChord=ms;
        }
        if(testChord&&int32_t(ms-nextTestChord)>=0){
            for(int interval:{0,4,7,11})emit(Event{Kind::Note,uint8_t(12*(octave+1)+interval),100});
            nextTestChord=ms+1000;
        }
        if(!reset&&!testChord)for(unsigned i=0;i<13;i++)if(input.notes[i]&&!blocked[i]&&midi[i]<0){
            midi[i]=12*(octave+1)+int(i);emit(Event{Kind::Note,uint8_t(midi[i]),100});
        }
        previous=input;
    }
};
}
