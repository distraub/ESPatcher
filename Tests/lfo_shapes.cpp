#include "../Firmware/lfo.h"
#include "../Firmware/arp.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    for(unsigned shape=0;shape<5;shape++)for(unsigned i=0;i<10000;i++){
        float phase=i/10000.f,value=gs_lfo_wave(phase,shape,.3f);assert(std::isfinite(value)&&std::abs(value)<=1.00001f);
        if(shape==0)assert(std::abs(value-std::sin(phase*6.28318530718f))<.0012f);
    }
    assert(gs_lfo_wave(.25f,1,0)==0&&gs_lfo_wave(.5f,1,0)==1&&gs_lfo_wave(0,1,0)==-1);
    assert(gs_lfo_wave(.25f,2,0)==1&&gs_lfo_wave(.75f,2,0)==-1);
    assert(gs_lfo_wave(0,3,0)==-1&&gs_lfo_wave(.5f,3,0)==0);
    for(unsigned sampleRate:{32000,44100,48000})for(unsigned division=0;division<9;division++){
        GSLFO lfo;gs_lfo_init(&lfo);float hz=gs_arp_synced_rate(137,division);
        for(unsigned i=0;i<sampleRate*2;i++)gs_lfo_advance(&lfo,hz,float(sampleRate));
        double expected=std::fmod(.25+2*double(hz),1.0);assert(std::abs(lfo.phase-expected)<1e-7);
        double before=lfo.phase;gs_lfo_advance(&lfo,gs_arp_synced_rate(90,division),float(sampleRate));
        double after=std::fmod(before+double(gs_arp_synced_rate(90,division))/sampleRate,1.0);assert(std::abs(lfo.phase-after)<1e-12);
    }
    GSLFO a,b;gs_lfo_init(&a);gs_lfo_init(&b);float held=a.held;
    for(unsigned i=0;i<20000;i++){
        double before=a.phase;gs_lfo_advance(&a,4,32000);gs_lfo_advance(&b,4,32000);
        assert(a.held==b.held&&a.phase==b.phase);
        if(a.phase>=before)assert(a.held==held);else assert(a.held!=held);
        held=a.held;
        uint32_t status=gs_lfo_monitor(&a,4);double phase=(status>>16)/65536.0;
        assert(std::abs(phase-a.phase)<=1.0/65536);
        float value=int16_t(status&65535)/32767.f;assert(std::abs(value-a.held)<.00004f);
    }
    std::cout<<"PASS: LFO shapes/sine error, synced divisions/phase at 32/44.1/48 kHz, live tempo continuity, sample/hold boundaries/determinism, phase/value telemetry\n";
}
