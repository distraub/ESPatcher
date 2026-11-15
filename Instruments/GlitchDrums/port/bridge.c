/* SPDX-License-Identifier: GPL-3.0-only */
#include "names.h"
#include "../upstream/platform/web.c"
#include "bridge.h"
#include "profile.h"
static int32_t gdp_pcm[512];
static float gdp_l[256],gdp_r[256];
EXPORT(gdp_boot) void gdp_boot(void){web_boot();}
EXPORT(gdp_tick) void gdp_tick(uint32_t ms){now_ms=ms;now_us=ms*1000u;input_snapshot();loop();screen_flush();}
EXPORT(gdp_audio) void gdp_audio(uint32_t frames){
 if(!frames||frames>256)return;
 gdp_profile_begin();
 #if GD_PORT_PROFILE
 uint32_t started=gdp_clock_us();
 #endif
 memset(gdp_pcm,0,frames*2*sizeof(int32_t));audio(gdp_pcm,frames);
 uint32_t gain=(web_master_v*web_master_v)>>8;
 for(unsigned i=0;i<frames;i++){
  gd_output_frame(&gd_output_state,&gdp_pcm[2*i],&gdp_pcm[2*i+1],gain);
  gdp_l[i]=gdp_pcm[2*i]/32768.f;gdp_r[i]=gdp_pcm[2*i+1]/32768.f;
 }
 #if GD_PORT_PROFILE
 gdp_profile_end(gdp_clock_us()-started);
 #endif
}
EXPORT(gdp_profile_peak) uint32_t gdp_profile_peak(uint32_t stage){
 #if GD_PORT_PROFILE
 return stage<13?gdp_peaks[stage]:0;
 #else
 (void)stage;return 0;
 #endif
}
EXPORT(gdp_left) float *gdp_left(void){return gdp_l;}
EXPORT(gdp_right) float *gdp_right(void){return gdp_r;}
EXPORT(gdp_screen) uint16_t *gdp_screen(void){return fb;}
EXPORT(gdp_screen_generation) uint32_t gdp_screen_generation(void){return fb_gen;}
EXPORT(gdp_keys) void gdp_keys(uint32_t held){web_keys(held);}
EXPORT(gdp_buttons) void gdp_buttons(uint32_t held){web_buttons(held);}
EXPORT(gdp_encoder) void gdp_encoder(uint32_t index,int32_t delta){web_encoder(index,delta);}
EXPORT(gdp_master) void gdp_master(uint32_t value){web_master(value);}
EXPORT(gdp_midi) void gdp_midi(uint32_t st,uint32_t a,uint32_t b){web_midi_in(st,a,b);}
EXPORT(gdp_key_leds) uint32_t gdp_key_leds(void){return led_k;}
EXPORT(gdp_button_leds) uint32_t gdp_button_leds(void){return led_b;}
EXPORT(gdp_state) uint8_t *gdp_state(void){return store;}
EXPORT(gdp_state_size) uint32_t gdp_state_size(void){return store_len<0?0:(uint32_t)store_len;}
EXPORT(gdp_state_revision) uint32_t gdp_state_revision(void){return store_gen;}
EXPORT(gdp_restore) int gdp_restore(uint32_t length){if(length>SETTINGS_MAX)return 0;store_len=(int32_t)length;return 1;}
extern int gdp_request_save(void);
EXPORT(gdp_save) int gdp_save(void){return gdp_request_save();}
