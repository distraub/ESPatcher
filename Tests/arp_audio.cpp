#include <array>
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
extern "C" {void analog_init(unsigned);void analog_parameter(unsigned,float);void analog_note(unsigned,unsigned);void analog_panic();float *analog_render(unsigned);unsigned analog_arp_status();}
static std::vector<float> capture(unsigned block){
    analog_init(32000);std::array<float,19> p{1,.005f,.25f,3000,.2f,.3f,1,0,1,3,7.3f,.4f,2,1,123,4,1,3,1};
    for(unsigned i=0;i<19;i++)analog_parameter(i,p[i]);for(unsigned n:{60,64,67})analog_note(n,100);
    std::vector<float> output;while(output.size()<64000){unsigned n=std::min(unsigned(64000-output.size()),block);auto samples=analog_render(n);output.insert(output.end(),samples,samples+n);}
    for(float x:output)assert(std::isfinite(x)&&std::abs(x)<.91f);return output;
}
int main(){
    auto a=capture(128),b=capture(37);assert(a==b);double energy=0;for(float x:a)energy+=x*x;assert(energy>1);
    assert(analog_arp_status()&0x80000000);
    for(unsigned n:{60,64,67})analog_note(n,0); // Latch continues after physical release.
    analog_render(128);assert((analog_arp_status()>>8&0x7fffff)>1);
    analog_parameter(8,0);assert((analog_arp_status()&0x80000000)==0);
    bool tail=false;for(unsigned b=0;b<100;b++){auto samples=analog_render(128);for(unsigned i=0;i<128;i++){if(b<10)tail=tail||samples[i]!=0;if(b==99)assert(samples[i]==0);}}assert(tail);
    analog_parameter(8,1);analog_render(128);analog_panic();auto silent=analog_render(512);for(unsigned i=0;i<512;i++)assert(silent[i]==0);
    analog_panic();analog_note(72,100);analog_note(72,0);analog_render(128);assert((analog_arp_status()&255)==73||(analog_arp_status()&255)==85);
    std::cout<<"PASS: arp audio block invariance, latch, stop release tails, panic clears latched chord, restart with new notes\n";
}
