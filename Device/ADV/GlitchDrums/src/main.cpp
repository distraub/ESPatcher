// SPDX-License-Identifier: GPL-3.0-only
// One owner serializes the full firmware. Speaker DMA runs independently.
#include <M5Cardputer.h>
#include "../../../../Instruments/GlitchDrums/port/bridge.h"
#include <algorithm>
#include <atomic>
static constexpr unsigned rate=44100,frames=256;
static int16_t pcm[3][frames*2];static std::atomic<bool> busy[3]{};
static unsigned buffer=0,knob=0,viewport=0;
static int master=700;static uint32_t previousTurn=0;
static const unsigned encoderMap[8]={0,0,1,6,2,3,4,5};
static uint32_t previousButtons=0,previousKeys=0,previousKnobs=0,lastDraw=0,lastReport=0;
static uint32_t dspMax=0,uiMax=0,late=0;
static void released(void*,const void *data,uint8_t){for(unsigned i=0;i<3;i++)if(data==pcm[i])busy[i].store(false);}
void setup(){
 Serial.begin(115200);auto config=M5.config();config.internal_mic=false;config.fallback_board=m5::board_t::board_M5CardputerADV;
 M5Cardputer.begin(config,true);M5Cardputer.Display.setRotation(1);M5Cardputer.Mic.end();M5Cardputer.Speaker.end();
 auto speaker=M5Cardputer.Speaker.config();speaker.sample_rate=rate;speaker.task_pinned_core=1;speaker.dma_buf_len=128;speaker.dma_buf_count=4;
 M5Cardputer.Speaker.config(speaker);M5Cardputer.Speaker.setBufferReleaseCallback(nullptr,released);M5Cardputer.Speaker.begin();M5Cardputer.Speaker.setVolume(180);
 gdp_boot();Serial.printf("Full GlitchDrums native: heap=%u. Hardware timing unverified.\n",ESP.getFreeHeap());
}
void loop(){
 M5Cardputer.update();uint32_t keys=0,buttons=0,numbers=0;const auto &state=M5Cardputer.Keyboard.keysState();
 // Original 27-key layout, reached through the Cardputer's three letter rows.
 const char *notes="qwertyuiopasdfghjklzxcvbnm,";
 for(char c:state.word){const char *at=strchr(notes,c);if(at)keys|=1u<<unsigned(at-notes);if(c>='1'&&c<='8')numbers|=1u<<(c-'1');if(c=='-')buttons|=1u<<12;if(c=='=')buttons|=1u<<13;}
 if(state.fn){keys=0;for(char c:state.word){switch(c){case 'c':buttons|=1u<<8;break;case 'v':buttons|=1u<<7;break;case 'b':buttons|=1u<<6;break;case 'n':buttons|=1u<<5;break;case 'm':buttons|=1u<<11;break;case 'q':buttons|=1u<<2;break;case 'w':buttons|=1u<<3;break;case 'e':buttons|=1u<<4;break;case 'r':buttons|=1u<<9;break;case 't':buttons|=1u<<10;break;case 'z':buttons|=1u<<0;break;case 'x':buttons|=1u<<1;break;case '.':viewport=105;break;case ',':viewport=0;break;}}}
 uint32_t changed=numbers&~previousKnobs;for(unsigned i=0;i<8;i++)if(changed&(1u<<i))knob=i;previousKnobs=numbers;
 // Hold Alt with comma/slash to turn a selected encoder; plain keys stay notes.
 uint32_t turns=0;if(state.alt){keys=0;for(char c:state.word){if(c==',')turns|=1;if(c=='/')turns|=2;}}
 uint32_t newTurns=turns&~previousTurn;previousTurn=turns;
 if(newTurns){int delta=(newTurns&2)?1:-1;if(knob==0){master=std::clamp(master+delta*16,0,1023);gdp_master(master);}else gdp_encoder(encoderMap[knob],delta);}

 if(keys!=previousKeys){gdp_keys(keys);previousKeys=keys;}if(buttons!=previousButtons){gdp_buttons(buttons);previousButtons=buttons;}
 uint32_t stamp=micros();gdp_tick(millis());uiMax=std::max(uiMax,uint32_t(micros()-stamp));
 if(!busy[buffer].load()&&M5Cardputer.Speaker.isPlaying(0)<2){stamp=micros();gdp_audio(frames);uint32_t us=micros()-stamp;dspMax=std::max(dspMax,us);if(us>5805)++late;
  for(unsigned i=0;i<frames;i++){pcm[buffer][2*i]=int16_t(std::clamp(gdp_left()[i],-1.f,1.f)*32767);pcm[buffer][2*i+1]=int16_t(std::clamp(gdp_right()[i],-1.f,1.f)*32767);}
  busy[buffer].store(true);if(!M5Cardputer.Speaker.playRaw(pcm[buffer],frames*2,rate,true,1,0,false))busy[buffer].store(false);buffer=(buffer+1)%3;
 }
 if(millis()-lastDraw>=50){lastDraw=millis();M5Cardputer.Display.pushImage(0,0,240,135,gdp_screen()+viewport*240);}
 if(millis()-lastReport>=2000){lastReport=millis();Serial.printf("DSP max=%luus UI max=%luus late=%lu heap=%u stack=%u\n",(unsigned long)dspMax,(unsigned long)uiMax,(unsigned long)late,ESP.getFreeHeap(),uxTaskGetStackHighWaterMark(nullptr));}
 delay(1);
}
