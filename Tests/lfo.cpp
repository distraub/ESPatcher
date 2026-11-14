#include <array>
#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>
extern "C" {void analog_init(unsigned);void analog_parameter(unsigned,float);void analog_note(unsigned,unsigned);float *analog_render(unsigned);}
static std::vector<float> capture(float depth,float hz,unsigned block,unsigned idle=0){
    analog_init(32000);
    std::array<float,8> p{1,.005f,.25f,1000,.2f,.4f,hz,depth};
    for(unsigned i=0;i<8;i++)analog_parameter(i,p[i]);
    analog_parameter(16,0);analog_parameter(18,1);
    while(idle){unsigned n=std::min(idle,block);auto out=analog_render(n);for(unsigned i=0;i<n;i++)assert(out[i]==0);idle-=n;}
    analog_note(48,100);std::vector<float> out;
    while(out.size()<64000){unsigned n=std::min(unsigned(64000-out.size()),block);auto samples=analog_render(n);out.insert(out.end(),samples,samples+n);}
    for(float x:out)assert(std::isfinite(x)&&std::abs(x)<=.9f);
    return out;
}
int main(){
    auto dry=capture(0,1,128),dryFast=capture(0,20,128);
    assert(dry==dryFast); // Rate has no effect at zero depth.
    auto wet=capture(.8f,1,128),split=capture(.8f,1,37);
    assert(wet==split); // LFO phase and control updates survive block boundaries.
    double difference=0;for(unsigned i=0;i<wet.size();i++)difference+=std::abs(wet[i]-dry[i]);assert(difference>50);
    auto idle=capture(.8f,1,128,8000);assert(idle!=wet); // Free-running during silence.
    capture(1,.05f,113);capture(1,20,113);
    std::cout<<"PASS: LFO zero-depth bypass, audible modulation, block invariance, silent free-running, bounded extreme rates\n";
}
