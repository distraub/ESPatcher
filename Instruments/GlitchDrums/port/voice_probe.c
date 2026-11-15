/* SPDX-License-Identifier: GPL-3.0-only
 * Eight unmodified voice engines and 14 voice slots. No UI, capture FX or I2S.
 * This is a focused CPU probe, not the full instrument or its sequencer. */
#include <stdint.h>
#include "../upstream/platform/fm1.h"
#include "../upstream/src/engine.h"
#include "profile.h"
/* ---- sound helpers */
int32_t sine(uint32_t phase) { /* a refined parabola, ~0.1 % */
  int32_t x = (int32_t)phase >> 16, ax = x < 0 ? -x : x, y, ay;
  y = (x * (32768 - ax)) >> 13; /* 4x(1-|x|), Q15 */
  ay = y < 0 ? -y : y;
  y += ((((y * ay) >> 15) - y) * 7373) >> 15; /* + 0.225 (y|y| - y) */
  return y > 32767 ? 32767 : y < -32767 ? -32767
                                        : y;
}

/* f * 2^32 / 44100 for MIDI 120..131 (C9..B9); lower octaves shift right */
static const uint32_t INC_C9[12] = {815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u, 1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};

uint32_t note_inc(int note) {
  if (note < 0) {
    note = 0;
  }
  if (note > 127) {
    note = 127;
  }
  return INC_C9[note % 12] >> (10 - note / 12);
}

static uint32_t random_state = 0x1234567u;
uint32_t random32(void) {
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}


static GDProject project;static GDEngine engine;static GDRenderBlock block;
static unsigned frameCount,step,remaining;
void gdv_boot(void){gd_defaults(&project);for(unsigned l=0;l<8;l++)engine.delay_send[l]=127;}
void gdv_render(void){
 gdp_profile_begin();uint32_t stamp=gdp_clock_us();unsigned left=256;
 while(left){if(!remaining){for(unsigned l=0;l<8;l++){unsigned v=project.track[l].step[step&15];if(v)gd_hit(&engine,&project,l,v,project.track[l].pitch[step&15],-1,0);}++step;remaining=44100*60/(project.bpm*4);}
 unsigned count=remaining<left?remaining:left;count=gd_render_block(&engine,&project,&block,count);remaining-=count;left-=count;frameCount+=count;}
 gdp_profile_end(gdp_clock_us()-stamp);
}
uint32_t gdv_peak(unsigned stage){return stage<13?gdp_peaks[stage]:0;}
