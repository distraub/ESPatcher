#include "../Core/WasmInstrument.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <chrono>
static std::vector<uint8_t> read(const char *name){std::ifstream f(name,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    auto analog=read("Firmware/Modules/pocket_analog.gsm5"),bell=read("Firmware/Modules/fm_bell.gsm5");
    gs::WasmInstrument a(analog,"pocket_analog",48000),b(bell,"fm_bell",48000),isolated(analog,"pocket_analog",48000);
    std::array<float,19> p{1,.015f,.25f,5000,.2f,0,1,0};a.parameters(p);b.parameters(p);isolated.parameters(p);
    a.noteOn(69,127);b.noteOn(69,127);float l[512],r[512],other[512];double difference=0,energy=0;
    auto start=std::chrono::steady_clock::now();
    for(int j=0;j<94;j++){a.render(l,r,512);b.render(other,nullptr,512);assert(!a.faulted()&&!b.faulted());for(int i=0;i<512;i++){assert(l[i]==r[i]);energy+=l[i]*l[i];difference+=std::abs(l[i]-other[i]);}}
    assert(energy>1&&difference>1);isolated.render(l,nullptr,512);for(float x:l)assert(x==0);
    a.panic();a.render(l,nullptr,512);for(float x:l)assert(x==0);
    bool refused=false;try{gs::WasmInstrument bad(analog,"wrong_id",48000);}catch(...){refused=true;}assert(refused);
    analog.resize(30);refused=false;try{gs::WasmInstrument bad(analog,"pocket_analog",48000);}catch(...){refused=true;}assert(refused);
    for(const char *fixture:{"Tests/Fixtures/runaway.gsm5","Tests/Fixtures/bounds.gsm5","Tests/Fixtures/trap.gsm5"}){
        gs::WasmInstrument bad(read(fixture),"pocket_analog",48000);bad.parameters(p);bad.noteOn(69,127);bad.render(l,nullptr,512);assert(bad.faulted());for(float x:l)assert(x==0);
    }
    gs::WasmInstrument legacy(read("Tests/Fixtures/legacy_mono.gsm5"),"pocket_analog",48000,5);
    assert(legacy.voiceLimit()==1&&legacy.voicesActive()==-1);legacy.parameters(p);legacy.noteOn(60,100);legacy.render(l,nullptr,512);assert(!legacy.faulted());
    std::cout<<"PASS: both WASM modules render distinct audio, isolated instances, panic, identity mismatch and truncated file rejection, infinite loop fuel, bounds and trap silencing, legacy mono compatibility; 2 seconds in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds\n";
}
