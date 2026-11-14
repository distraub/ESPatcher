/* Portable adaptation of GlitchDrums' GDLadderFilter by Alex Matheu.
 * Same four-stage TPT feedback solve; ESP32-friendly tanh approximation.
 * No allocation or platform APIs. All voices share coefficients and a table.
 */
#pragma once
#include <math.h>
#define GS_LADDER_PI 3.14159265358979323846f
#define GS_LADDER_TABLE_SIZE 256u
typedef struct { float s[4]; } GSLadderState;
typedef struct { float g,g2,g3,g4,k,one_minus_g,inverse; } GSLadderCoefficients;
static float gs_ladder_table[GS_LADDER_TABLE_SIZE+1];
static int gs_ladder_ready=0;
static void gs_ladder_init(void){
    if(gs_ladder_ready)return;
    for(unsigned i=0;i<=GS_LADDER_TABLE_SIZE;i++){
        float x=(3.f/GS_LADDER_TABLE_SIZE)*i,x2=x*x;
        gs_ladder_table[i]=x*(27.f+x2)/(27.f+9.f*x2);
    }
    gs_ladder_ready=1;
}
static float gs_ladder_saturate(float x){
    float magnitude=fabsf(x);if(magnitude>=3.f)return copysignf(1.f,x);
    float position=magnitude*(GS_LADDER_TABLE_SIZE/3.f);
    unsigned index=(unsigned)position;float fraction=position-index;
    float y=gs_ladder_table[index]+fraction*(gs_ladder_table[index+1]-gs_ladder_table[index]);
    return copysignf(y,x);
}
/* Rational prewarp from GDLadderFilter's fastTan path. */
static float gs_ladder_target(float cutoff,float rate){
    cutoff=fminf(fmaxf(cutoff,80.f),rate*.45f);
    float x=GS_LADDER_PI*cutoff/rate,x2=x*x;
    float tangent=x*(10395.f+x2*(-1260.f+21.f*x2))/(10395.f+x2*(-4725.f+x2*(210.f-x2)));
    return tangent/(1.f+tangent);
}
static void gs_ladder_coefficients(GSLadderCoefficients *c,float g,float feedback){
    c->g=g;c->g2=g*g;c->g3=c->g2*g;c->g4=c->g2*c->g2;c->k=feedback;c->one_minus_g=1.f-g;
    c->inverse=1.f/(1.f+feedback*c->g4);
}
static float gs_ladder_process(GSLadderState *state,const GSLadderCoefficients *c,float x){
    float s1=state->s[0]*c->one_minus_g,s2=state->s[1]*c->one_minus_g;
    float s3=state->s[2]*c->one_minus_g,s4=state->s[3]*c->one_minus_g;
    float y=(x-c->k*(c->g3*s1+c->g2*s2+c->g*s3+s4))*c->inverse;
    for(unsigned stage=0;stage<4;stage++){
        float v=(gs_ladder_saturate(y)-state->s[stage])*c->g;
        y=v+state->s[stage];state->s[stage]=y+v;
    }
    return y;
}
