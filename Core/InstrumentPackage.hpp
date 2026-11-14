#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace gs {
constexpr uint32_t packageLimit=4*1024*1024;
struct PackageScene {
    std::string title,subtitle;
    uint8_t layout=0; // 0 scope, 1 LFO, 2 values, 3 levels
    std::array<uint8_t,8> bindings{};
    std::array<std::string,8> labels{};
    std::array<uint8_t,16> pads{};
};
struct PackageEngine {
    std::string id,name;
    uint32_t memory=0,moduleOffset=0,moduleSize=0;
    std::array<uint8_t,32> digest{};
    std::array<float,19> defaults{};
    unsigned sceneCount=0;
    std::array<PackageScene,4> scenes;
};
struct InstrumentPackage {
    std::string id,name,version;
    unsigned engineCount=0;
    std::array<PackageEngine,4> engines;
};
inline bool packageID(const std::string& id){
    if(id.empty()||id.size()>48)return false;
    for(char c:id)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    return true;
}
template<class Read> struct PackageCursor {
    Read read;uint32_t size,pos=0;
    void bytes(void *destination,uint32_t count){
        if(count>size-pos||!read(pos,destination,count))throw std::runtime_error("Truncated package");pos+=count;
    }
    uint8_t u8(){uint8_t value;bytes(&value,1);return value;}
    uint32_t u32(){uint8_t b[4];bytes(b,4);return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
    std::string text(unsigned maximum){
        auto n=u8();if(!n||n>maximum)throw std::runtime_error("Invalid package text length");
        std::string result(n,' ');bytes(&result[0],n);
        for(unsigned char c:result)if(c<32||c>126)throw std::runtime_error("Package text must be printable ASCII");return result;
    }
    float number(){uint32_t raw=u32();float value;std::memcpy(&value,&raw,4);if(!std::isfinite(value))throw std::runtime_error("Nonfinite package value");return value;}
    void skip(uint32_t count){if(count>size-pos)throw std::runtime_error("Truncated module");pos+=count;}
};
// Random-access reader lets SD scan metadata without allocating module bytes.
// Call on the control thread into an unpublished candidate; publish only on success.
template<class Read> void readInstrumentPackage(Read read,uint32_t size,InstrumentPackage& package){
    if(size<16||size>packageLimit)throw std::runtime_error("Package size outside prototype limits");
    PackageCursor<Read> c{read,size};char magic[8];c.bytes(magic,8);
    if(std::memcmp(magic,"ESPKG001",8)||c.u32()!=size)throw std::runtime_error("Unsupported package header");
    package.id=c.text(48);package.name=c.text(48);package.version=c.text(16);
    if(!packageID(package.id))throw std::runtime_error("Invalid package identity");
    package.engineCount=c.u8();if(!package.engineCount||package.engineCount>4)throw std::runtime_error("Invalid engine count");
    for(unsigned e=0;e<package.engineCount;e++){
        auto& engine=package.engines[e];engine.id=c.text(48);engine.name=c.text(48);
        if(!packageID(engine.id))throw std::runtime_error("Invalid engine identity");
        for(unsigned other=0;other<e;other++)if(package.engines[other].id==engine.id)throw std::runtime_error("Duplicate engine identity");
        engine.memory=c.u32();if(engine.memory!=65536&&engine.memory!=131072&&engine.memory!=262144)throw std::runtime_error("Unsupported engine memory");
        static const float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
        for(unsigned p=0;p<19;p++){
            engine.defaults[p]=c.number();float value=engine.defaults[p];
            bool discrete=p==0||p==8||p==9||p==12||p==13||p==15||p>=16;
            if(value<lo[p]||value>hi[p]||(discrete&&std::floor(value)!=value))throw std::runtime_error("Invalid parameter default");
        }
        // Packages cannot start transport simply by being opened.
        if(engine.defaults[8]!=0)throw std::runtime_error("Package transport must default to stopped");
        engine.sceneCount=c.u8();if(!engine.sceneCount||engine.sceneCount>4)throw std::runtime_error("Invalid scene count");
        for(unsigned s=0;s<engine.sceneCount;s++){
            auto& scene=engine.scenes[s];scene.title=c.text(24);scene.subtitle=c.text(48);scene.layout=c.u8();
            if(scene.layout>3)throw std::runtime_error("Unknown scene layout");
            c.bytes(scene.bindings.data(),8);
            for(auto binding:scene.bindings)if(binding!=255&&binding>=19)throw std::runtime_error("Unknown parameter binding");
            for(auto& label:scene.labels)label=c.text(8);
            c.bytes(scene.pads.data(),16);for(auto note:scene.pads)if(note>48)throw std::runtime_error("Invalid pad note offset");
        }
        engine.moduleSize=c.u32();if(engine.moduleSize<8||engine.moduleSize>1024*1024)throw std::runtime_error("Invalid module size");
        c.bytes(engine.digest.data(),32);engine.moduleOffset=c.pos;c.skip(engine.moduleSize);
    }
    if(c.pos!=size)throw std::runtime_error("Unexpected trailing package data");
}
}
