/* Shared waveform math and audio-clock phase; UI consumes published snapshots. */
#pragma once
#include <stdint.h>
#include <math.h>
typedef struct {double phase,increment;float last_hz,last_rate,held;uint32_t random;} GSLFO;
static float gs_lfo_random(GSLFO *l){l->random^=l->random<<13;l->random^=l->random>>17;l->random^=l->random<<5;return (float)(l->random>>8)*(2.f/16777215.f)-1;}
static void gs_lfo_init(GSLFO *l){l->phase=.25;l->increment=0;l->last_hz=l->last_rate=-1;l->random=0x6d2b79f5;l->held=gs_lfo_random(l);}
static void gs_lfo_advance(GSLFO *l,float hz,float rate){if(l->last_hz!=hz||l->last_rate!=rate){l->increment=(double)hz/rate;l->last_hz=hz;l->last_rate=rate;}l->phase+=l->increment;if(l->phase>=1){l->phase-=1;l->held=gs_lfo_random(l);}}
static float gs_lfo_wave(float phase,unsigned shape,float held){
    if(shape==0){float x=phase*2;if(x>1)x-=2;float y=4*x*(1-fabsf(x));return y+.225f*(y*fabsf(y)-y);}
    if(shape==1)return 1-4*fabsf(phase-.5f);
    if(shape==2)return phase<.5f?1.f:-1.f;
    if(shape==3)return 2*phase-1;
    return held;
}
static uint32_t gs_lfo_monitor(const GSLFO *l,unsigned shape){float value=gs_lfo_wave((float)l->phase,shape,l->held);return ((uint32_t)(l->phase*65536)<<16)|(uint16_t)(int16_t)(value*32767);}
