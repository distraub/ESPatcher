#include "../Core/InstrumentPackage.hpp"
#include "../Core/WasmInstrument.hpp"
#include <CommonCrypto/CommonDigest.h>
#include <fstream>
#include <iterator>
#include <cassert>
#include <iostream>
#include <chrono>
static std::vector<uint8_t> file(const char *path){std::ifstream stream(path,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
static gs::InstrumentPackage parse(const std::vector<uint8_t>& bytes){
    gs::InstrumentPackage package;
    gs::readInstrumentPackage([&](uint32_t offset,void *target,uint32_t count){if(offset>bytes.size()||count>bytes.size()-offset)return false;std::memcpy(target,bytes.data()+offset,count);return true;},uint32_t(bytes.size()),package);return package;
}
static void verify(const std::vector<uint8_t>& bytes,const gs::PackageEngine& engine){
    uint8_t sha[32];CC_SHA256(bytes.data()+engine.moduleOffset,engine.moduleSize,sha);
    if(std::memcmp(sha,engine.digest.data(),32))throw std::runtime_error("Checksum mismatch");
}
int main(){
    unsigned switches=0;double energy=0;uint64_t maxUs=0;
    for(unsigned round=0;round<6;round++)for(auto path:{"Library/amsynth.espkg","Library/orbit-study.espkg","Library/signal-check.espkg"}){
        auto bytes=file(path);auto package=parse(bytes);
        assert(package.engineCount>0&&package.engineCount<=4);
        for(unsigned index=0;index<package.engineCount;index++){
            auto& engine=package.engines[index];verify(bytes,engine);
            std::vector<uint8_t> module(bytes.begin()+engine.moduleOffset,bytes.begin()+engine.moduleOffset+engine.moduleSize);
            bool legacyRejected=false;try{gs::WasmInstrument::inspect(module);}catch(...){legacyRejected=true;}assert(legacyRejected);
            gs::WasmInstrument vm(std::move(module),engine.id,32000,19,65536,16384);
            vm.parameters(engine.defaults);for(int note:{60,64,67,70})vm.noteOn(note,100);
            for(unsigned block=0;block<32;block++){
                float samples[256];auto start=std::chrono::steady_clock::now();vm.render(samples,nullptr,256);
                maxUs=std::max(maxUs,uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()));
                for(float sample:samples){assert(std::isfinite(sample)&&std::abs(sample)<=.9f);energy+=sample*sample;}
                assert(!vm.faulted());
            }
            vm.panic();assert(engine.scenes[0].bindings[0]<19);switches++;
        }
    }
    assert(energy>1&&switches==30);
    auto bytes=file("Library/amsynth.espkg");auto package=parse(bytes);
    for(size_t size:{size_t(0),size_t(8),size_t(12),size_t(32),bytes.size()-1}){
        bool rejected=false;try{parse(std::vector<uint8_t>(bytes.begin(),bytes.begin()+size));}catch(...){rejected=true;}assert(rejected);
    }
    const uint8_t title[]={4,'P','L','A','Y'};
    auto at=std::search(bytes.begin(),bytes.end(),std::begin(title),std::end(title));assert(at!=bytes.end());
    size_t subtitle=size_t(at-bytes.begin())+5,layout=subtitle+1+bytes[subtitle];
    for(auto change:{std::pair<size_t,uint8_t>(layout,4),{layout+1,19}}){
        auto invalid=bytes;invalid[change.first]=change.second;bool bad=false;try{parse(invalid);}catch(...){bad=true;}assert(bad);
    }
    auto damaged=bytes;damaged[package.engines[0].moduleOffset+10]^=1;
    auto metadata=parse(damaged);bool rejected=false;try{verify(damaged,metadata.engines[0]);}catch(...){rejected=true;}assert(rejected);
    auto orbit=parse(file("Library/orbit-study.espkg"));assert(orbit.engines[0].scenes[0].pads[13]==2&&orbit.engines[0].scenes[0].labels[0]=="MOTION");
    std::cout<<"PASS packages: 30 RAM-only engine loads, three instrument UIs, checksum/truncation rejection, 64 KiB isolation; desktop max block "<<maxUs<<" us (not device timing)\n";
}
