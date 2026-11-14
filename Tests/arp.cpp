#include "../Firmware/arp.h"
#include <vector>
#include <array>
#include <cassert>
#include <iostream>
#include <algorithm>
struct Event{unsigned sample,note,velocity;};
static unsigned sample=0;static std::vector<Event> events;
static void emit(unsigned n,unsigned v){events.push_back({sample,n,v});}
static std::vector<unsigned> pattern(unsigned mode){
    GSArp arp;gs_arp_init(&arp);gs_arp_config(&arp,32000,mode,4,.5f,2,0);
    for(unsigned n:{60,64,67})gs_arp_input(&arp,n,100);
    gs_arp_config(&arp,32000,mode,4,.5f,2,0);gs_arp_run(&arp,1);events.clear();
    for(sample=0;sample<12*8000;sample++)gs_arp_tick(&arp,emit);
    std::vector<unsigned> notes;for(auto e:events)if(e.velocity)notes.push_back(e.note);
    assert(notes.size()==12);return notes;
}
int main(){
    assert(gs_arp_synced_rate(120,3)==4);
    assert(gs_arp_synced_rate(120,5)==8);
    assert(gs_arp_synced_rate(60,3)==2);
    assert(gs_arp_synced_rate(240,3)==8);
    for(unsigned division=0;division<9;division++)for(unsigned sampleRate:{32000,44100,48000}){
        GSArp synced;gs_arp_init(&synced);gs_arp_input(&synced,60,100);
        gs_arp_config(&synced,float(sampleRate),0,gs_arp_synced_rate(123,division),.5f,1,0);gs_arp_run(&synced,1);events.clear();
        for(sample=0;sample<sampleRate*2;sample++)gs_arp_tick(&synced,emit);
        unsigned count=0;for(auto event:events)if(event.velocity){assert(std::abs(double(event.sample)-count*synced.interval)<1.01);count++;}
        double fraction=synced.until_step/synced.interval;
        gs_arp_config(&synced,float(sampleRate),0,gs_arp_synced_rate(90,division),.5f,1,0);
        assert(std::abs(fraction-synced.until_step/synced.interval)<1e-8);
    }

    auto up=pattern(0),down=pattern(1),bounce=pattern(2),random=pattern(3);
    assert((std::vector<unsigned>(up.begin(),up.begin()+6)==std::vector<unsigned>{60,64,67,72,76,79}));
    assert((std::vector<unsigned>(down.begin(),down.begin()+6)==std::vector<unsigned>{79,76,72,67,64,60}));
    assert((std::vector<unsigned>(bounce.begin(),bounce.begin()+11)==std::vector<unsigned>{60,64,67,72,76,79,76,72,67,64,60}));
    assert(random==pattern(3));for(unsigned n:random)assert(std::find(up.begin(),up.end(),n)!=up.end());
    for(unsigned rate:{32000,44100,48000}){
        GSArp arp;gs_arp_init(&arp);gs_arp_input(&arp,60,100);gs_arp_config(&arp,float(rate),0,7.3f,.25f,1,0);gs_arp_run(&arp,1);events.clear();
        for(sample=0;sample<rate*4;sample++)gs_arp_tick(&arp,emit);
        unsigned count=0;for(auto e:events)if(e.velocity){assert(std::abs(double(e.sample)-count*arp.interval)<=1.01);count++;}
        assert(count>29);assert(events[1].velocity==0&&std::abs(double(events[1].sample)-arp.interval*.25)<=1.01);
    }
    GSArp arp;gs_arp_init(&arp);gs_arp_config(&arp,32000,0,4,.5f,1,1);
    gs_arp_input(&arp,60,100);gs_arp_input(&arp,64,100);gs_arp_input(&arp,60,0);gs_arp_input(&arp,64,0);gs_arp_config(&arp,32000,0,4,.5f,1,1);assert(arp.count==2);
    gs_arp_input(&arp,67,100);gs_arp_config(&arp,32000,0,4,.5f,1,1);assert(arp.count==1&&arp.pool[0]==67);
    gs_arp_input(&arp,67,0);gs_arp_config(&arp,32000,0,4,.5f,1,0);assert(arp.count==0);
    gs_arp_input(&arp,127,100);gs_arp_config(&arp,32000,2,20,1,4,0);assert(arp.count==1&&arp.pool[0]==127);
    gs_arp_run(&arp,1);events.clear();sample=0;gs_arp_tick(&arp,emit);assert(events.size()==1&&events[0].note==127);
    gs_arp_input(&arp,127,0);gs_arp_config(&arp,32000,2,20,1,4,0);sample++;gs_arp_tick(&arp,emit);assert(events.back().velocity==0);
    gs_arp_input(&arp,60,100);gs_arp_config(&arp,32000,0,4,.5f,1,0);sample++;gs_arp_tick(&arp,emit);double fraction=arp.until_step/arp.interval;
    gs_arp_config(&arp,32000,0,8,.5f,1,0);assert(std::abs(fraction-arp.until_step/arp.interval)<1e-8);
    std::cout<<"PASS: arp patterns, deterministic random, octave bounds, sample-clock timing/gate at 32/44.1/48 kHz, latch replacement/release and rate phase continuity\n";
}
