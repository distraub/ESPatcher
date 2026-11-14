#include "../Firmware/ladder.h"
#include "Reference/GDLadderFilter.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

static double response(int rate,float cutoff,float resonance,float frequency,bool reference=false){
    GSLadderState state{};GSLadderCoefficients coefficients{};
    gs_ladder_coefficients(&coefficients,gs_ladder_target(cutoff,float(rate)),resonance*4.2f);
    GlitchDrums::GDLadderFilter original;original.prepare(rate);original.setCutoff(cutoff);original.setResonance(resonance);
    double energy=0;for(int i=0;i<rate;i++){
        float x=.01f*std::sin(float(6.283185307179586*frequency*i/rate));
        float y=reference?original.process(x):gs_ladder_process(&state,&coefficients,x);
        if(i>=rate/2)energy+=y*y;
    }
    return std::sqrt(energy/(rate-rate/2));
}
int main(){
    gs_ladder_init();double maxError=0;
    for(int i=-8000;i<=8000;i++){
        float x=i*.001f,y=gs_ladder_saturate(x);assert(std::isfinite(y)&&std::abs(y)<=1);
        maxError=std::max(maxError,double(std::abs(y-std::tanh(x))));
    }
    assert(maxError<.025);
    for(int rate:{8000,16000,32000,44100,48000,96000,192000}){
        for(float resonance:{0.f,.5f,.9f,1.f}){
            GSLadderState state{};GSLadderCoefficients coefficients{};uint32_t random=7;
            for(int i=0;i<32000;i++){
                float cutoff=(i/32)%2?80.f:std::min(16000.f,rate*.45f);
                gs_ladder_coefficients(&coefficients,gs_ladder_target(cutoff,float(rate)),resonance*4.2f);
                random=random*1664525u+1013904223u;float x=float(random>>8)/8388608.f-1;
                float y=gs_ladder_process(&state,&coefficients,x);assert(std::isfinite(y)&&std::abs(y)<8);
                for(float s:state.s)assert(std::isfinite(s)&&std::abs(s)<64);
            }
        }
    }
    for(int rate:{32000,44100,48000}){
        double flat=response(rate,1000,0,1000),peak=response(rate,1000,.8f,1000);
        assert(peak>flat*3);
        double low=response(rate,500,0,2000),high=response(rate,500,0,4000);assert(low>high*10);
        for(float resonance:{0.f,.5f,.8f}){
            double port=response(rate,1000,resonance,1000),original=response(rate,1000,resonance,1000,true);
            assert(std::abs(port-original)/original<.15);
        }
        GSLadderState state{};GSLadderCoefficients coefficients{};
        gs_ladder_coefficients(&coefficients,gs_ladder_target(1000,rate),4.2f);
        double ring=0;for(int i=0;i<rate;i++){
            float y=gs_ladder_process(&state,&coefficients,i==0?.1f:0.f);
            if(i>rate/2)ring+=y*y;
        }
        assert(ring/(rate/2)>.000001); // Seeded self-oscillation sustains without input.
        auto before=state;gs_ladder_coefficients(&coefficients,gs_ladder_target(2000,rate),3.f);
        for(int i=0;i<4;i++)assert(state.s[i]==before.s[i]); // Control edits do not reset integrators.
    }
    std::cout<<"PASS: ladder resonance peak, four-pole rolloff, original GlitchDrums response comparison, self-oscillation, saturation error <0.025, state preservation, extreme cutoff switching at 8–192 kHz\n";
}
