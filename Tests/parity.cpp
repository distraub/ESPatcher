#include "../Core/WasmInstrument.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
extern "C" {
void analog_init(uint32_t);void analog_note(uint32_t,uint32_t);void analog_parameter(uint32_t,float);float *analog_render(uint32_t);
void bell_init(uint32_t);void bell_note(uint32_t,uint32_t);void bell_parameter(uint32_t,float);float *bell_render(uint32_t);
}
int main(){
    for(int which=0;which<2;which++)for(int sampleRate:{32000,44100,48000})for(float resonance:{0.f,.8f,1.f})for(float playing:{0.f,1.f})for(int lfoShape=0;lfoShape<5;lfoShape++){
        const char *id=which?"fm_bell":"pocket_analog";
        std::ifstream file(std::string("Firmware/Modules/")+id+".gsm5",std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
        gs::WasmInstrument wasm(bytes,id,sampleRate);
        auto init=which?bell_init:analog_init;auto note=which?bell_note:analog_note;auto param=which?bell_parameter:analog_parameter;auto render=which?bell_render:analog_render;
        init(sampleRate);std::array<float,19> parameters{1,.015f,.25f,5000,.2f,resonance,2,.7f,playing,2,7.3f,.4f,2,1,123,4,1,3,float(lfoShape)};
        for(int i=0;i<19;i++)param(i,parameters[i]);wasm.parameters(parameters);
        for(int n:{60,64,72}){
            note(n,100);wasm.noteOn(n,100);
            for(int block=0;block<20;block++){
                float output[128];wasm.render(output,nullptr,128);auto native=render(128);
                assert(!wasm.faulted());for(int i=0;i<128;i++)assert(std::abs(output[i]-native[i])<.0002f);
            }
            note(n,0);wasm.noteOff(n);
        }
        init(sampleRate);wasm.prepare(sampleRate);wasm.parameters(parameters);
        for(int n:{48,52,55,59,60,64}){note(n,100);wasm.noteOn(n,100);}
        for(int block=0;block<80;block++){
            if(block==20){parameters[14]=90;parameters[15]=8;param(14,90);param(15,8);wasm.parameters(parameters);note(52,0);wasm.noteOff(52);}
            if(block==40){parameters[3]=800;parameters[5]=.9f;param(3,800);param(5,.9f);wasm.parameters(parameters);note(72,100);wasm.noteOn(72,100);}
            if(block==60){parameters[8]=0;param(8,0);wasm.parameters(parameters);note(64,0);wasm.noteOff(64);}
            float output[128];wasm.render(output,nullptr,128);auto native=render(128);
            assert(!wasm.faulted()&&wasm.voiceLimit()==4&&wasm.voicesActive()<=4);
            for(int i=0;i<128;i++)assert(std::abs(output[i]-native[i])<.0002f);
        }
    }
    std::cout<<"PASS: both modules' native C and WASM mono/chord/steal audio agree within 0.0002 at 32/44.1/48 kHz\n";
}
