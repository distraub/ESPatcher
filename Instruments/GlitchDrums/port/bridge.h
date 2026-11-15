/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t gdp_profile_peak(uint32_t stage);
void gdp_boot(void); /* once per instance, fixed 44.1 kHz */
void gdp_tick(uint32_t milliseconds);
void gdp_audio(uint32_t frames); /* stereo float, 1..256 */
float *gdp_left(void);
float *gdp_right(void);
uint16_t *gdp_screen(void); /* byte-swapped RGB565, 240 x 240 */
uint32_t gdp_screen_generation(void);
void gdp_keys(uint32_t held);
void gdp_buttons(uint32_t held);
void gdp_encoder(uint32_t index,int32_t delta);
void gdp_master(uint32_t value);
void gdp_midi(uint32_t status,uint32_t d1,uint32_t d2);
uint32_t gdp_key_leds(void);
uint32_t gdp_button_leds(void);
uint8_t *gdp_state(void);
uint32_t gdp_state_size(void);
uint32_t gdp_state_revision(void);
int gdp_restore(uint32_t length); /* copy state bytes before boot */
int gdp_save(void);
#ifdef __cplusplus
}
#endif
