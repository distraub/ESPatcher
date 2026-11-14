/* Fixed-size, sample-clocked arpeggiator. No platform calls or allocations. */
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
static float gs_arp_synced_rate(float bpm,unsigned division){
    static const float beats[]={4,2,1,.5f,1.f/3,.25f,1.f/6,.125f,1.f/12};
    return bpm/(60*beats[division<9?division:3]);
}
typedef struct {
    uint8_t held[128],chord[128],pool[512],velocity[512];
    unsigned count,index,step,pattern,octaves,latch,running,dirty;
    uint32_t random;
    int current;
    double interval,until_step,until_gate;
    float gate;
} GSArp;
static void gs_arp_init(GSArp *a){memset(a,0,sizeof *a);a->current=-1;a->random=0x12345678;a->octaves=1;a->gate=.5f;a->dirty=1;}
static unsigned gs_arp_held(const GSArp *a){unsigned n=0;for(unsigned i=0;i<128;i++)n+=a->held[i]!=0;return n;}
static void gs_arp_input(GSArp *a,unsigned note,unsigned velocity){
    if(note>127)return;
    if(velocity){if(a->latch&&!gs_arp_held(a))memset(a->chord,0,sizeof a->chord);a->held[note]=(uint8_t)velocity;a->chord[note]=(uint8_t)velocity;}
    else{a->held[note]=0;if(!a->latch)a->chord[note]=0;}
    a->dirty=1;
}
static void gs_arp_config(GSArp *a,float sample_rate,unsigned pattern,float hz,float gate,unsigned octaves,unsigned latch){
    double interval=sample_rate/hz;
    if(a->interval>0&&a->interval!=interval){a->until_step*=interval/a->interval;a->until_gate*=interval/a->interval;}
    a->interval=interval;
    if(a->gate!=gate&&a->current>=0)a->until_gate=interval*gate-(interval-a->until_step);
    a->gate=gate;
    if(a->pattern!=pattern){a->pattern=pattern;a->index=0;}
    if(a->octaves!=octaves){a->octaves=octaves;a->dirty=1;}
    if(a->latch!=latch){a->latch=latch;memcpy(a->chord,a->held,sizeof a->chord);a->dirty=1;}
    if(a->dirty){
        a->count=0;
        // Sort by pitch across octaves, including non-triad input chords.
        for(unsigned note=0;note<128;note++)for(unsigned octave=0;octave<a->octaves;octave++){
            int base=(int)note-(int)octave*12;
            if(base>=0&&a->chord[base]){a->pool[a->count]=(uint8_t)note;a->velocity[a->count++]=a->chord[base];}
        }
        if(!a->count){a->index=0;a->until_step=0;}
        a->dirty=0;
    }
}
static void gs_arp_run(GSArp *a,unsigned running){
    a->running=running;a->current=-1;a->until_step=a->until_gate=0;a->index=a->step=0;a->random=0x12345678;
}
typedef void (*GSArpEmit)(unsigned note,unsigned velocity);
static void gs_arp_tick(GSArp *a,GSArpEmit emit){
    if(!a->running)return;
    if(a->current>=0&&(a->until_gate<=0||!a->count)){emit((unsigned)a->current,0);a->current=-1;}
    if(!a->count)return;
    if(a->until_step<=0){
        if(a->current>=0)emit((unsigned)a->current,0);
        unsigned index=0;
        if(a->pattern==0)index=a->index%a->count;
        else if(a->pattern==1)index=a->count-1-a->index%a->count;
        else if(a->pattern==2){unsigned length=a->count>1?2*a->count-2:1;unsigned p=a->index%length;index=p<a->count?p:length-p;}
        else{a->random^=a->random<<13;a->random^=a->random>>17;a->random^=a->random<<5;index=a->random%a->count;}
        a->current=a->pool[index];emit((unsigned)a->current,a->velocity[index]);a->index++;a->step++;
        a->until_step+=a->interval;a->until_gate=a->interval*a->gate;
    }
    a->until_step-=1;a->until_gate-=1;
}
static uint32_t gs_arp_monitor(const GSArp *a){return (a->running?0x80000000u:0)|((a->step&0x7fffffu)<<8)|(uint32_t)(a->current+1);}
