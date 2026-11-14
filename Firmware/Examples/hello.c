/* Minimal SD-runtime sanity instrument. Original code, MIT, Alex Matheu.
 * Only level, attack and release are implemented; Pulse also uses shape for duty.
 * Remaining prototype parameter slots are deliberately no-ops. */
#include <stdint.h>
#include <math.h>
#include <string.h>
#ifndef INSTRUMENT
#define INSTRUMENT 0
#endif
static float table[257],output[4096];
static struct {float phase,step,level,velocity;unsigned note,held,age;} voices[4];
static float rate=32000,volume=.2f,attack=.015f,release=.25f,up,down;
static unsigned age=0,shape=1;
uint32_t gs_version(void){return 1;}
const char *gs_id(void){return INSTRUMENT?"hello_pulse":"hello_tone";}
uint32_t gs_voice_limit(void){return 4;}
uint32_t gs_active_voices(void){unsigned n=0;for(unsigned i=0;i<4;i++)n+=voices[i].held||voices[i].level>0;return n;}
uint32_t gs_lfo_status(void){return 0;}
uint32_t gs_arp_status(void){return 0;}
void gs_panic(void){memset(voices,0,sizeof voices);}
void gs_init(uint32_t sampleRate){
 rate=sampleRate>=8000&&sampleRate<=192000?sampleRate:32000;
 up=1.f/fmaxf(1,attack*rate);down=1.f/fmaxf(1,release*rate);age=0;gs_panic();
 for(unsigned i=0;i<=256;i++)table[i]=sinf(i*6.283185307179586f/256);
}
void gs_parameter(uint32_t address,float value){
 if(!isfinite(value))return;
 if(address==0)shape=(unsigned)fminf(2,fmaxf(0,roundf(value)));
 if(address==1){attack=fminf(2,fmaxf(.005f,value));up=1.f/fmaxf(1,attack*rate);}
 if(address==2){release=fminf(3,fmaxf(.01f,value));down=1.f/fmaxf(1,release*rate);}
 if(address==4)volume=fminf(.5f,fmaxf(0,value));
}
void gs_note(uint32_t note,uint32_t velocity){
 if(note>127||velocity>127)return;
 if(!velocity){for(unsigned i=0;i<4;i++)if(voices[i].note==note)voices[i].held=0;return;}
 unsigned chosen=0;
 for(unsigned i=0;i<4;i++){
  if(voices[i].held&&voices[i].note==note){chosen=i;break;}
  if(!voices[i].held&&voices[i].level==0){chosen=i;break;}
  if(voices[i].age<voices[chosen].age)chosen=i;
 }
 voices[chosen].phase=0;voices[chosen].step=fminf(.45f,440.f*powf(2.f,((int)note-69)/12.f)/rate);
 voices[chosen].level=0;voices[chosen].velocity=velocity/127.f;voices[chosen].note=note;voices[chosen].held=1;voices[chosen].age=++age;
}
float *gs_render(uint32_t frames){
 if(frames>4096)return 0;
 for(unsigned sample=0;sample<frames;sample++){
  float sum=0;
  for(unsigned i=0;i<4;i++){
   if(!voices[i].held&&voices[i].level<=0)continue;
   voices[i].level=voices[i].held?fminf(1,voices[i].level+up):fmaxf(0,voices[i].level-down);
   float wave;
   if(INSTRUMENT)wave=voices[i].phase<(.25f+.25f*shape)?1:-1;
   else {float at=voices[i].phase*256;unsigned index=(unsigned)at;wave=table[index]+(table[index+1]-table[index])*(at-index);}
   sum+=wave*voices[i].level*voices[i].velocity*.2f;voices[i].phase+=voices[i].step;if(voices[i].phase>=1)voices[i].phase-=1;
  }
  output[sample]=sum*volume;
 }
 return output;
}
