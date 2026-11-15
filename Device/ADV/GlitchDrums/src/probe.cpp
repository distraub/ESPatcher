// SPDX-License-Identifier: GPL-3.0-only
#include <Arduino.h>
#include <algorithm>
extern "C" {void gdv_boot(void);void gdv_render(void);uint32_t gdv_peak(unsigned);uint32_t gdp_clock_us(void){return micros();}}
static uint32_t blocks=0,worst=0;static uint64_t total=0;
void setup(){Serial.begin(115200);delay(1000);gdv_boot();}
void loop(){uint32_t started=micros();gdv_render();uint32_t us=micros()-started;worst=std::max(worst,us);total+=us;++blocks;
 if(blocks%1723==0){Serial.printf("VOICE ENGINE PROBE ONLY / NO FX, UI OR I2S: blocks=%lu mean_us=%lu max_us=%lu deadline_us=5805 heap=%u stack=%u\n",(unsigned long)blocks,(unsigned long)(total/blocks),(unsigned long)worst,ESP.getFreeHeap(),uxTaskGetStackHighWaterMark(nullptr));const char *names[]={"KICK","SNARE","CH","OH","CLAP","PERC","GLITCH","ANALOG","CTRL","HITS","FX","OTHER","TOTAL"};for(unsigned i=0;i<13;i++)Serial.printf("PEAK %s=%luus ",names[i],(unsigned long)gdv_peak(i));Serial.println();}delay(0);}
