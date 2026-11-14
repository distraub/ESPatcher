#pragma once
#include "../Vendor/wasm3/wasm3.h"
#include "../Vendor/wasm3/gs_sandbox.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace gs {
// Independent ABI, inspired by RemoteFM1's manifest + WASM-export design.
class WasmInstrument {
    std::vector<uint8_t> bytes;
    IM3Environment environment=nullptr;
    IM3Runtime runtime=nullptr;
    IM3Function initFn=nullptr,noteFn=nullptr,paramFn=nullptr,panicFn=nullptr,renderFn=nullptr;
    IM3Function voiceCountFn=nullptr,arpStatusFn=nullptr,lfoStatusFn=nullptr;
    uint32_t arpState=0,lfoState=0;
    uint32_t limit=1,activeVoices=0;
    std::array<float,19> last{{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}};
    unsigned parameterCount=19;
    bool failed=false;
    const char *trap=nullptr;
    static uint32_t integer(const uint8_t *&p,const uint8_t *end){
        uint32_t value=0;for(unsigned i=0;i<5;i++){if(p==end)throw std::runtime_error("Truncated WASM integer");uint8_t b=*p++;if(i==4&&(b&0xf0))throw std::runtime_error("Invalid WASM integer");value|=uint32_t(b&127)<<(7*i);if(!(b&128))return value;}throw std::runtime_error("Invalid WASM integer");
    }
    IM3Function function(const char *name,std::initializer_list<M3ValueType> args,M3ValueType result=c_m3Type_none){
        IM3Function f=nullptr;check(m3_FindFunction(&f,runtime,name));
        if(m3_GetArgCount(f)!=args.size()||m3_GetRetCount(f)!=(result==c_m3Type_none?0:1))throw std::runtime_error(std::string("Wrong export signature: ")+name);
        unsigned i=0;for(auto t:args)if(m3_GetArgType(f,i++)!=t)throw std::runtime_error(std::string("Wrong export argument: ")+name);
        if(result!=c_m3Type_none&&m3_GetRetType(f,0)!=result)throw std::runtime_error("Wrong export result");return f;
    }
    static void check(M3Result error){if(error)throw std::runtime_error(error);}
    void budget(uint64_t fuel=1000000){gs_m3_budget(runtime,fuel);}
    bool accept(M3Result error){if(error){failed=true;trap=error;}return !error;}
    void destroy(){if(runtime)m3_FreeRuntime(runtime);if(environment)m3_FreeEnvironment(environment);runtime=nullptr;environment=nullptr;}
public:
    std::string manifest,id;
    static std::string inspect(const std::vector<uint8_t>& data){
        if(data.size()<8||data.size()>1024*1024||std::memcmp(data.data(),"\0asm\1\0\0\0",8))throw std::runtime_error("Not a supported .gsm5 WebAssembly module");
        auto p=data.data()+8,end=data.data()+data.size();std::string manifest;unsigned memories=0,manifests=0;
        while(p<end){uint8_t section=*p++;uint32_t length=integer(p,end);if(length>uint32_t(end-p))throw std::runtime_error("Truncated WASM section");auto limit=p+length;
            if(section==0){auto n=integer(p,limit);if(n>uint32_t(limit-p))throw std::runtime_error("Invalid custom section");if(n==3&&!std::memcmp(p,"gs5",3)){if(++manifests!=1)throw std::runtime_error("Duplicate manifest");p+=3;if(limit-p>8192)throw std::runtime_error("Manifest too large");manifest.assign((const char*)p,size_t(limit-p));}}
            if(section==1||section==3||section==6||section==7||section==9||section==11){
                auto cursor=p;auto count=integer(cursor,limit);
                uint32_t maximum=(section==1||section==3)?512:(section==11?128:64);
                if(count>maximum)throw std::runtime_error("Firmware section count exceeds ABI limits");
            }
            if(section==4){
                auto cursor=p;auto count=integer(cursor,limit);if(count>1)throw std::runtime_error("Too many tables");
                if(count){if(cursor==limit||*cursor++!=0x70)throw std::runtime_error("Unsupported table type");auto flags=integer(cursor,limit);auto initial=integer(cursor,limit);auto max=flags==1?integer(cursor,limit):initial;if(flags>1||initial>64||max>64)throw std::runtime_error("Firmware table too large");}
            }
            if(section==2){if(integer(p,limit)!=0)throw std::runtime_error("Firmware imports are not allowed");}
            if(section==5){if(++memories!=1||integer(p,limit)!=1||integer(p,limit)!=1||integer(p,limit)!=4||integer(p,limit)!=4||p!=limit)throw std::runtime_error("Firmware requires fixed 256 KiB memory");}
            if(section==8)throw std::runtime_error("WASM start functions are not allowed");p=limit;
        }
        if(manifest.empty()||memories!=1)throw std::runtime_error("Missing gs5 manifest or memory");return manifest;
    }
    WasmInstrument(std::vector<uint8_t> data,const std::string& expectedId,double sampleRate,unsigned parameters=19):bytes(std::move(data)),parameterCount(parameters){
        try{
            if(parameterCount!=5&&parameterCount!=6&&parameterCount!=8&&parameterCount!=14&&parameterCount!=16&&parameterCount!=19)throw std::runtime_error("Unsupported parameter count");
            manifest=inspect(bytes);environment=m3_NewEnvironment();if(!environment)throw std::runtime_error("Cannot allocate WASM environment");
            runtime=m3_NewRuntime(environment,65536,nullptr);if(!runtime)throw std::runtime_error("Cannot allocate WASM runtime");gs_m3_limit(runtime,262144);budget();
            IM3Module module=nullptr;check(m3_ParseModule(environment,&module,bytes.data(),uint32_t(bytes.size())));
            auto loaded=m3_LoadModule(runtime,module);if(loaded){m3_FreeModule(module);check(loaded);}check(m3_CompileModule(module));
            IM3Function initialize=nullptr;if(!m3_FindFunction(&initialize,runtime,"_initialize")){initialize=function("_initialize",{});budget();check(m3_CallV(initialize));}
            auto version=function("gs_version",{},c_m3Type_i32);budget();check(m3_CallV(version));uint32_t abi=0;check(m3_GetResultsV(version,&abi));if(abi!=1)throw std::runtime_error("Unsupported firmware ABI");
            auto identity=function("gs_id",{},c_m3Type_i32);budget();check(m3_CallV(identity));uint32_t ptr=0,size=0;check(m3_GetResultsV(identity,&ptr));auto memory=m3_GetMemory(runtime,&size,0);
            if(ptr>=size||!std::memchr(memory+ptr,0,std::min(64u,size-ptr)))throw std::runtime_error("Invalid firmware identity");id=(const char*)memory+ptr;if(id!=expectedId)throw std::runtime_error("Manifest and firmware identity disagree");
            initFn=function("gs_init",{c_m3Type_i32});noteFn=function("gs_note",{c_m3Type_i32,c_m3Type_i32});paramFn=function("gs_parameter",{c_m3Type_i32,c_m3Type_f32});panicFn=function("gs_panic",{});renderFn=function("gs_render",{c_m3Type_i32},c_m3Type_i32);
            IM3Function limitFn=nullptr;
            if(!m3_FindFunction(&limitFn,runtime,"gs_voice_limit")){
                limitFn=function("gs_voice_limit",{},c_m3Type_i32);budget();check(m3_CallV(limitFn));check(m3_GetResultsV(limitFn,&limit));
                if(limit<1||limit>8)throw std::runtime_error("Unsupported firmware voice limit");
            }
            if(!m3_FindFunction(&voiceCountFn,runtime,"gs_active_voices"))voiceCountFn=function("gs_active_voices",{},c_m3Type_i32);
            else voiceCountFn=nullptr;
            if(!m3_FindFunction(&arpStatusFn,runtime,"gs_arp_status"))arpStatusFn=function("gs_arp_status",{},c_m3Type_i32);else arpStatusFn=nullptr;
            if(!m3_FindFunction(&lfoStatusFn,runtime,"gs_lfo_status"))lfoStatusFn=function("gs_lfo_status",{},c_m3Type_i32);else lfoStatusFn=nullptr;
            if(parameterCount==19&&!lfoStatusFn)throw std::runtime_error("LFO status export required");
            if(parameterCount>=14&&!arpStatusFn)throw std::runtime_error("Arpeggiator status export is required for synth-v4");
            budget();check(m3_CallV(initFn,uint32_t(sampleRate)));
            // Force all paths before publication; VM compilation/allocation stays off render.
            budget();check(m3_CallV(noteFn,uint32_t(60),uint32_t(100)));float scratch[128];render(scratch,nullptr,128);if(failed)throw std::runtime_error(trap?trap:"Invalid firmware output");prepare(sampleRate);
        }catch(...){destroy();throw;}
    }
    ~WasmInstrument(){destroy();}
    WasmInstrument(const WasmInstrument&)=delete;WasmInstrument& operator=(const WasmInstrument&)=delete;
    void prepare(double rate){budget();check(m3_CallV(initFn,uint32_t(rate)));last.fill(-1);activeVoices=0;arpState=lfoState=0;}
    uint32_t lfoStatus()const{return lfoState;}
    uint32_t arpStatus()const{return arpState;}
    uint32_t voiceLimit()const{return limit;}
    int voicesActive()const{return voiceCountFn?int(activeVoices):-1;}
    bool faulted()const{return failed;}
    void parameters(const std::array<float,19>& values){if(failed)return;for(unsigned i=0;i<parameterCount;i++)if(last[i]!=values[i]){budget(100000);if(!accept(m3_CallV(paramFn,uint32_t(i),double(values[i]))))return;last[i]=values[i];}}
    void noteOn(int note,int velocity){if(failed)return;budget(100000);accept(m3_CallV(noteFn,uint32_t(note),uint32_t(velocity)));}
    void noteOff(int note){noteOn(note,0);}
    void panic(){if(failed)return;budget(100000);accept(m3_CallV(panicFn));}
    void render(float *left,float *right,unsigned frames){
        if(failed){std::fill_n(left,frames,0);if(right)std::fill_n(right,frames,0);return;}
        for(unsigned offset=0;offset<frames;){unsigned n=std::min(128u,frames-offset);budget(1500000);if(!accept(m3_CallV(renderFn,uint32_t(n))))break;
            uint32_t ptr=0,size=0;if(!accept(m3_GetResultsV(renderFn,&ptr)))break;auto memory=m3_GetMemory(runtime,&size,0);
            if(ptr%4||ptr>size||n*sizeof(float)>size-ptr){failed=true;break;}auto samples=(float*)(memory+ptr);
            for(unsigned i=0;i<n;i++){if(!std::isfinite(samples[i])){failed=true;break;}float x=std::clamp(samples[i],-.9f,.9f);left[offset+i]=x;if(right)right[offset+i]=x;}
            if(failed)break;offset+=n;
        }
        if(!failed&&voiceCountFn){budget(10000);if(accept(m3_CallV(voiceCountFn)))accept(m3_GetResultsV(voiceCountFn,&activeVoices));if(activeVoices>limit)failed=true;}
        if(!failed&&arpStatusFn){budget(10000);if(accept(m3_CallV(arpStatusFn)))accept(m3_GetResultsV(arpStatusFn,&arpState));}
        if(!failed&&lfoStatusFn){budget(10000);if(accept(m3_CallV(lfoStatusFn)))accept(m3_GetResultsV(lfoStatusFn,&lfoState));}
        if(failed){lfoState=0;arpState=0;activeVoices=0;std::fill_n(left,frames,0);if(right)std::fill_n(right,frames,0);}
    }
};
}
