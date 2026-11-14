#include "../Core/Synth.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main(){
    for(double rate:{44100.,48000.})for(int wave=0;wave<3;wave++){
        gs::Synth synth;synth.prepare(rate);synth.params.wave=wave;
        std::vector<float> l(static_cast<size_t>(rate), 0.f),r(l.size());
        synth.noteOn(69,127);synth.render(l.data(),r.data(),unsigned(l.size()));
        double energy=0;unsigned crossings=0;
        for(size_t i=0;i<l.size();i++){assert(std::isfinite(l[i]));assert(std::abs(l[i])<=.51);assert(l[i]==r[i]);energy+=l[i]*l[i];if(i&&l[i-1]<=0&&l[i]>0)crossings++;}
        assert(energy>1);assert(crossings>=438&&crossings<=441);
        synth.noteOff(69);synth.render(l.data(),r.data(),unsigned(l.size()));
        assert(l.back()==0);
        synth.noteOn(127,127);synth.params.cutoff=16000;synth.render(l.data(),nullptr,unsigned(l.size()));
        for(float v:l)assert(std::isfinite(v)&&std::abs(v)<.51);
        synth.panic();synth.render(l.data(),nullptr,unsigned(l.size()));for(float v:l)assert(v==0);
    }
    gs::Synth a,b;a.prepare(48000);b.prepare(48000);a.noteOn(60,100);b.noteOn(60,100);
    float whole[1024],split[1024];a.render(whole,nullptr,1024);
    for(int i=0;i<1024;i+=128)b.render(split+i,nullptr,128);
    for(int i=0;i<1024;i++)assert(whole[i]==split[i]);
    a.noteOn(64,100);a.noteOff(64);a.render(whole,nullptr,1024);
    b.render(split,nullptr,1024); // Earlier held note returns with phase continuity.
    for(int i=0;i<1024;i++)assert(whole[i]==split[i]);
    std::cout<<"PASS: pitch, finite bounded stereo, release, panic, block-size parity, held-note return at 44.1/48 kHz\n";
}
