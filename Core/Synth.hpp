#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gs {
// Render-thread owned. No platform APIs, heap allocation, locks, or I/O.
struct Parameters { int wave=1; float attack=.015f, release=.25f, cutoff=5000, volume=.2f; };
class Synth {
    double rate=48000, phase=0, increment=0;
    float level=0, velocity=1, low=0, smoothGain=0, smoothCoefficient=0;
    bool gate=false;
    std::array<uint8_t,128> held{};
    std::array<uint8_t,128> order{};
    unsigned count=0;
    float blep(double t) const {
        if(t<increment){double x=t/increment;return float(x+x-x*x-1);}
        if(t>1-increment){double x=(t-1)/increment;return float(x*x+x+x+1);}
        return 0;
    }
    void tune(int note){increment=std::min(.45,440.0*std::pow(2.0,(note-69)/12.0)/rate);}
public:
    Parameters params;
    void prepare(double sr){rate=std::clamp(sr,8000.0,192000.0);phase=0;low=0;smoothGain=0;smoothCoefficient=0;panic();}
    void panic(){held.fill(0);count=0;gate=false;level=0;}
    void noteOn(int note,int vel){
        if(note<0||note>127)return;
        if(vel<=0){noteOff(note);return;}
        noteOff(note);held[note]=1;order[count++]=uint8_t(note);
        velocity=std::clamp(vel,1,127)/127.f;gate=true;tune(note);
    }
    void noteOff(int note){
        if(note<0||note>127||!held[note])return;
        held[note]=0;unsigned j=0;for(unsigned i=0;i<count;i++)if(order[i]!=note)order[j++]=order[i];count=j;
        gate=count>0;if(gate)tune(order[count-1]);
    }
    void render(float* left,float* right,unsigned frames){
        const float attackStep=1.f/(std::clamp(params.attack,.005f,2.f)*rate);
        const float releaseStep=1.f/(std::clamp(params.release,.01f,3.f)*rate);
        const float coefficient=1-std::exp(float(-6.28318530718*std::clamp(params.cutoff,80.f,float(rate*.45))/rate));
        const float gain=std::clamp(params.volume,0.f,.5f);
        const float smoothing=1-std::exp(float(-1/(.01*rate)));
        for(unsigned i=0;i<frames;i++){
            level=gate?std::min(1.f,level+attackStep):std::max(0.f,level-releaseStep);
            float x;
            if(params.wave==0)x=std::sin(float(phase*6.28318530718));
            else if(params.wave==2)x=(phase<.5?1.f:-1.f)+blep(phase)-blep(std::fmod(phase+.5,1.0));
            else x=float(2*phase-1)-blep(phase);
            phase+=increment;if(phase>=1)phase-=1;
            smoothGain+=smoothing*(gain-smoothGain);smoothCoefficient+=smoothing*(coefficient-smoothCoefficient);
            low+=smoothCoefficient*(x-low);
            float y=low*level*velocity*smoothGain;
            left[i]=y;if(right)right[i]=y;
        }
    }
};
}
