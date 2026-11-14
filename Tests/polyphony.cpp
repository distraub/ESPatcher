#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
extern "C" {
void analog_init(uint32_t);void analog_note(uint32_t,uint32_t);void analog_parameter(uint32_t,float);void analog_panic();float *analog_render(uint32_t);uint32_t analog_voice_limit();uint32_t analog_active_voices();
void bell_init(uint32_t);void bell_note(uint32_t,uint32_t);void bell_parameter(uint32_t,float);void bell_panic();float *bell_render(uint32_t);uint32_t bell_voice_limit();uint32_t bell_active_voices();
}
double amplitude(const std::vector<float>& signal,int note,int rate){
    double frequency=440*std::pow(2.,(note-69)/12.),real=0,imaginary=0;
    for(size_t i=0;i<signal.size();i++){double phase=6.283185307179586*frequency*i/rate;real+=signal[i]*std::cos(phase);imaginary+=signal[i]*std::sin(phase);}
    return 2*std::hypot(real,imaginary)/signal.size();
}
int main(){
    for(int rate:{32000,44100,48000}){
        analog_init(rate);analog_parameter(0,0);analog_parameter(1,.005f);analog_parameter(2,.01f);analog_parameter(3,16000);analog_parameter(4,.2f);
        for(int note:{60,64,67,71})analog_note(note,127);
        assert(analog_voice_limit()==4&&analog_active_voices()==4);
        analog_render(256);
        auto capture=[&](){std::vector<float> audio;for(int i=0;i<rate/256;i++){auto out=analog_render(128);audio.insert(audio.end(),out,out+128);}return audio;};
        auto audio=capture();for(int note:{60,64,67,71})assert(amplitude(audio,note,rate)>.02);
        analog_note(72,127);analog_render(256);audio=capture();
        assert(amplitude(audio,60,rate)<.003);for(int note:{64,67,71,72})assert(amplitude(audio,note,rate)>.02);
        analog_note(60,0); // A note-off for the stolen pitch must not release its replacement.
        for(int note:{64,67,71})analog_note(note,0);
        analog_render(4096);assert(analog_active_voices()==1);analog_note(72,0);analog_render(4096);assert(analog_active_voices()==0);
        auto silentAnalog=analog_render(256);for(int i=0;i<256;i++)assert(silentAnalog[i]==0);
        analog_init(rate);for(int note:{60,64,67,71})analog_note(note,127);analog_render(256);
        analog_note(71,0);analog_note(72,127);analog_render(256);audio=capture();
        assert(amplitude(audio,60,rate)>.02&&amplitude(audio,71,rate)<.003); // Release tail is taken before oldest held key.
        analog_panic();assert(analog_active_voices()==0);
        for(int shape=0;shape<3;shape++){
            analog_parameter(0,float(shape));analog_parameter(4,.5f);bell_init(rate);bell_parameter(0,float(shape));bell_parameter(4,.5f);
            for(int note=0;note<128;note++){
                analog_note(note,127);bell_note(note,127);
                auto a=analog_render(128),b=bell_render(128);assert(analog_active_voices()<=4&&bell_active_voices()<=4);
                for(int i=0;i<128;i++)assert(std::isfinite(a[i])&&std::abs(a[i])<=.9f&&std::isfinite(b[i])&&std::abs(b[i])<=.9f);
            }
            bell_panic();assert(bell_active_voices()==0);auto silent=bell_render(128);for(int i=0;i<128;i++)assert(silent[i]==0);
            analog_panic();
        }
    }
    std::cout<<"PASS: four simultaneous pitches, oldest-held stealing, release-tail preference, stolen note-off isolation, release-to-idle, all MIDI pitches/shapes bounded, panic at 32/44.1/48 kHz\n";
}
