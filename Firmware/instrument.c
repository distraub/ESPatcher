/* Four-voice portable instruments. Original code by Alex Matheu. */
#include "firmware.h"
#include "ladder.h"
#include "arp.h"
#include "lfo.h"
#include <math.h>
#include <string.h>
#ifndef INSTRUMENT
#define INSTRUMENT 0
#endif
#ifndef GS_VOICE_COUNT
#define GS_VOICE_COUNT 4
#endif
#if GS_VOICE_COUNT < 1 || GS_VOICE_COUNT > 8
#error GS_VOICE_COUNT must be between 1 and 8
#endif
#define PI_F 3.14159265358979323846f
#define SINE_SIZE 1024u
#define STEAL_FADE 32u

typedef struct {
    float phase,mod_phase,step,level,velocity,strike;
    GSLadderState filter;
    float last,fade_from;
    uint32_t age;
    uint8_t note,active,held,fade;
} Voice;
static Voice voices[GS_VOICE_COUNT];
static float output[4096],sine_table[SINE_SIZE+1];
static float parameters[19]={1,.015f,.25f,5000,.2f,0,1,0,0,0,4,.5f,1,0,120,3,1,1,1};
static float rate=48000,gain=0,coefficient=0,feedback=0;
static GSLadderCoefficients ladder;
static GSArp arp;
static GSLFO lfo;
static float lfo_rate=1,lfo_depth=0,lfo_target=0;
static unsigned lfo_tick=0;
static uint32_t serial=0;
static int table_ready=0;
static float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static float sine(float phase){
    /* All callers keep the input between -0.5 and 1.5 cycles. */
    if(phase<0)phase+=1;
    if(phase>=1)phase-=1;
    float position=phase*SINE_SIZE;unsigned index=(unsigned)position;
    float fraction=position-index;
    return sine_table[index]+fraction*(sine_table[index+1]-sine_table[index]);
}
static float blep(float t,float step){
    if(t<step){float x=t/step;return x+x-x*x-1;}
    if(t>1-step){float x=(t-1)/step;return x*x+x+x+1;}return 0;
}
uint32_t gs_version(void){return 1;}
const char *gs_id(void){return INSTRUMENT?"fm_bell":"pocket_analog";}
uint32_t gs_voice_limit(void){return GS_VOICE_COUNT;}
uint32_t gs_active_voices(void){unsigned count=0;for(unsigned i=0;i<GS_VOICE_COUNT;i++)count+=voices[i].active!=0;return count;}
uint32_t gs_lfo_status(void){return gs_lfo_monitor(&lfo,(unsigned)(parameters[18]+.5f));}
uint32_t gs_arp_status(void){return gs_arp_monitor(&arp);}
void gs_panic(void){memset(voices,0,sizeof voices);serial=0;unsigned running=arp.running,latch=arp.latch;gs_arp_init(&arp);arp.running=running;arp.latch=latch;}
void gs_init(uint32_t sample_rate){
    rate=clamp(sample_rate,8000,192000);gs_lfo_init(&lfo);lfo_rate=1;lfo_depth=0;lfo_tick=0;gain=coefficient=feedback=0;gs_arp_init(&arp);gs_panic();gs_arp_run(&arp,parameters[8]>=.5f);gs_ladder_init();
    /* One-time table generation, never in the sample loop or on note-on. */
    if(!table_ready){for(unsigned i=0;i<=SINE_SIZE;i++)sine_table[i]=sinf((2*PI_F*i)/SINE_SIZE);table_ready=1;}
}
static void voice_note(unsigned note,unsigned velocity){
    if(note>127)return;
    if(!velocity){for(unsigned i=0;i<GS_VOICE_COUNT;i++)if(voices[i].active&&voices[i].note==note)voices[i].held=0;return;}
    if(!gs_active_voices()){
        gain=parameters[4];coefficient=gs_ladder_target(parameters[3],rate);feedback=4.2f*parameters[5];
        gs_ladder_coefficients(&ladder,coefficient,feedback);
    }
    Voice *chosen=0;
    /* Retrigger the same held pitch, then use an idle slot. */
    for(unsigned i=0;i<GS_VOICE_COUNT;i++)if(voices[i].active&&voices[i].held&&voices[i].note==note){chosen=&voices[i];break;}
    if(!chosen)for(unsigned i=0;i<GS_VOICE_COUNT;i++)if(!voices[i].active){chosen=&voices[i];break;}
    /* Prefer a quiet release tail over interrupting a held key. */
    if(!chosen){float quietest=2;for(unsigned i=0;i<GS_VOICE_COUNT;i++)if(!voices[i].held){float score=voices[i].level*voices[i].velocity*(INSTRUMENT?voices[i].strike:1);if(score<quietest){quietest=score;chosen=&voices[i];}}}
    /* Otherwise steal the oldest held note; unsigned ages survive wraparound. */
    if(!chosen){uint32_t oldest=0;for(unsigned i=0;i<GS_VOICE_COUNT;i++){uint32_t age=serial-voices[i].age;if(!chosen||age>oldest){oldest=age;chosen=&voices[i];}}}
    float tail=chosen->active?chosen->last:0;
    memset(chosen,0,sizeof *chosen);
    chosen->active=chosen->held=1;chosen->note=(uint8_t)note;chosen->age=++serial;
    chosen->velocity=clamp(velocity,1,127)/127.f;chosen->strike=1;
    chosen->step=fminf(.45f,(440.f*powf(2.f,((int)note-69)/12.f))/rate);
    chosen->fade_from=tail;chosen->fade=tail!=0?STEAL_FADE:0;
}
void gs_note(uint32_t note,uint32_t velocity){
    if(note>127)return;
    gs_arp_input(&arp,note,(unsigned)clamp(velocity,0,127));
    if(!arp.running)voice_note(note,velocity);
}
void gs_parameter(uint32_t address,float value){
    static const float lo[19]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[19]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
    if(address<19&&isfinite(value)){
        parameters[address]=clamp(value,lo[address],hi[address]);
        if(address==8&&arp.running!=(unsigned)(parameters[8]>=.5f)){
            // Stop/re-route held voices without erasing their release tails.
            for(unsigned i=0;i<GS_VOICE_COUNT;i++)voices[i].held=0;
            gs_arp_run(&arp,parameters[8]>=.5f);
        }
        if(address==13)gs_arp_config(&arp,rate,(unsigned)(parameters[9]+.5f),gs_arp_synced_rate(parameters[14],(unsigned)(parameters[15]+.5f)),parameters[11],(unsigned)(parameters[12]+.5f),parameters[13]>=.5f);
    }
}
float *gs_render(uint32_t frames){
    if(frames>4096)return 0;
#if GS_TEST_BAD
    int bad_note=0;for(unsigned i=0;i<GS_VOICE_COUNT;i++)if(voices[i].active&&voices[i].note==69)bad_note=1;
#if GS_TEST_BAD == 1
    if(bad_note){volatile unsigned spin=0;for(;;)spin++;}
#elif GS_TEST_BAD == 2
    if(bad_note)return (float *)(uintptr_t)262140;
#elif GS_TEST_BAD == 3
    if(bad_note)__builtin_trap();
#endif
#endif

    gs_arp_config(&arp,rate,(unsigned)(parameters[9]+.5f),gs_arp_synced_rate(parameters[14],(unsigned)(parameters[15]+.5f)),parameters[11],(unsigned)(parameters[12]+.5f),parameters[13]>=.5f);
    /* Expensive coefficients are shared by all voices and calculated per block. */
    float attack=1/(parameters[1]*rate),release=1/(parameters[2]*rate);
    float target=gs_ladder_target(parameters[3],rate),target_feedback=4.2f*parameters[5];
    float smooth=1-expf(-1/(.01f*rate)),decay=expf(-1/(parameters[2]*rate));
    int shape=(int)(parameters[0]+.5f);
    float target_rate=parameters[16]>=.5f?gs_arp_synced_rate(parameters[14],(unsigned)(parameters[17]+.5f)):parameters[6];
    for(unsigned i=0;i<frames;i++){
        gs_arp_tick(&arp,voice_note);
        float sum=0;
        if(parameters[16]>=.5f)lfo_rate=target_rate;else lfo_rate+=smooth*(target_rate-lfo_rate);
        lfo_depth+=smooth*(parameters[7]-lfo_depth);
        gs_lfo_advance(&lfo,lfo_rate,rate);
        // One shared triangle LFO; at most +/- three octaves of cutoff travel.
        // Update the expensive mapping at 1/16 sample rate, then smooth it.
        if(lfo_tick==0){
            float modulation=gs_lfo_wave((float)lfo.phase,(unsigned)(parameters[18]+.5f),lfo.held);
            float cutoff=parameters[3]*exp2f(modulation*lfo_depth*3);
            lfo_target=gs_ladder_target(clamp(cutoff,80,fminf(16000,.45f*rate)),rate);
        }
        lfo_tick=(lfo_tick+1)&15;
        float filter_target=(parameters[7]==0&&lfo_depth<.000001f)?target:lfo_target;
        gain+=smooth*(parameters[4]-gain);
        coefficient+=smooth*(filter_target-coefficient);feedback+=smooth*(target_feedback-feedback);
        if(coefficient!=ladder.g||feedback!=ladder.k)gs_ladder_coefficients(&ladder,coefficient,feedback);
        for(unsigned j=0;j<GS_VOICE_COUNT;j++){
            Voice *v=&voices[j];if(!v->active)continue;
            v->level=v->held?fminf(1,v->level+attack):fmaxf(0,v->level-release);
            float x;
            if(INSTRUMENT){
                x=sine(v->phase+(3.f/(2*PI_F))*v->strike*sine(v->mod_phase))*v->strike;
                v->strike*=decay;v->mod_phase+=v->step*(shape+1);
                while(v->mod_phase>=1)v->mod_phase-=1;
            }else if(shape==0)x=sine(v->phase);
            else if(shape==2){float shifted=v->phase+.5f;if(shifted>=1)shifted-=1;x=(v->phase<.5f?1.f:-1.f)+blep(v->phase,v->step)-blep(shifted,v->step);}
            else x=2*v->phase-1-blep(v->phase,v->step);
            v->phase+=v->step;if(v->phase>=1)v->phase-=1;
            float y=gs_ladder_process(&v->filter,&ladder,x)*v->level*v->velocity;
            if(v->fade){float old=v->fade/(float)STEAL_FADE;y=y*(1-old)+v->fade_from*old;--v->fade;}
            v->last=y;sum+=y;
            if((!v->held&&v->level<=0&&!v->fade)||(INSTRUMENT&&v->strike<.00001f&&!v->fade))v->active=0;
        }
        /* Fixed 6 dB headroom: note releases never pump the remaining voices. */
        float y=sum*gain*.5f,magnitude=fabsf(y);
        if(magnitude>.75f){float excess=magnitude-.75f;y=copysignf(.75f+.15f*excess/(.15f+excess),y);}
        output[i]=y;
    }
    return output;
}
