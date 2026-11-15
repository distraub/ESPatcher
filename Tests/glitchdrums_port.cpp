// SPDX-License-Identifier: GPL-3.0-only
#include "../Instruments/GlitchDrums/port/bridge.h"
#include <chrono>
#include <time.h>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <fstream>
#include <algorithm>
using Clock=std::chrono::steady_clock;
extern "C" uint32_t gdp_clock_us(void){struct timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint32_t(uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000);}
int main(int argc,char**argv){
 gdp_boot();
 auto t0=Clock::now();gdp_buttons(1u<<12);gdp_tick(1);gdp_buttons(0);gdp_tick(2);
 double peak=0,energy=0,totalUs=0,maxUs=0,uiUs=0,uiMax=0;unsigned ms=2;uint64_t frames=0;
 std::ofstream pcm;if(argc>1)pcm.open(argv[1],std::ios::binary);
 for(unsigned block=0;block<1723;block++){
  while(ms<frames*1000/44100){auto a=Clock::now();gdp_tick(++ms);double us=std::chrono::duration<double,std::micro>(Clock::now()-a).count();uiUs+=us;uiMax=std::max(uiMax,us);}
  auto a=Clock::now();gdp_audio(256);auto b=Clock::now();
  double us=std::chrono::duration<double,std::micro>(b-a).count();totalUs+=us;maxUs=std::max(maxUs,us);
  for(unsigned i=0;i<256;i++){
   float l=gdp_left()[i],r=gdp_right()[i];assert(std::isfinite(l)&&std::isfinite(r));
   peak=std::max(peak,std::max(std::abs(double(l)),std::abs(double(r))));energy+=l*l+r*r;
   if(pcm){int16_t pair[2]={int16_t(std::clamp(l,-1.f,1.f)*32767),int16_t(std::clamp(r,-1.f,1.f)*32767)};pcm.write((char*)pair,sizeof pair);}
  }frames+=256;
 }
 assert(energy>1&&peak<=1.001);assert(gdp_screen_generation()>0);
 // Every screen remains reachable, including original sequencer/effects/presets.
 for(unsigned b=2;b<=11;b++){gdp_buttons(1u<<b);gdp_tick(++ms);gdp_buttons(0);gdp_tick(++ms);}
 gdp_save();assert(gdp_state_size()>0);
 double elapsed=std::chrono::duration<double>(Clock::now()-t0).count();
 std::printf("PASS full GlitchDrums: 8 lanes, stock sequence, all screen buttons, finite stereo and project save\n");
 std::printf("HOST ONLY: %.3fs audio in %.3fs wall; DSP mean %.1fus max %.1fus; deadline 5805us; peak %.3f; saved %u bytes\n",frames/44100.,elapsed,totalUs/1723,maxUs,peak,gdp_state_size());
 const char *stages[]={"KICK","SNARE","CH","OH","CLAP","PERC","GLITCH","ANALOG","CTRL","HITS","FX","OTHER","TOTAL"};
 for(unsigned i=0;i<13;i++)std::printf("PROFILE independent thread-CPU peak %s %uus\n",stages[i],gdp_profile_peak(i));
 std::printf("HOST UI ticks total %.1fus max %.1fus; instrumentation overhead included\n",uiUs,uiMax);
}
