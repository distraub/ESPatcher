/* AMSynth module ABI 1. Original code by Alex Matheu. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t gs_version(void);
const char *gs_id(void);
void gs_init(uint32_t sample_rate);
void gs_note(uint32_t note, uint32_t velocity); /* velocity 0 = note off */
void gs_parameter(uint32_t address, float value);
void gs_panic(void);
uint32_t gs_voice_limit(void); /* Optional ABI 1 diagnostic. */
uint32_t gs_active_voices(void);
uint32_t gs_lfo_status(void); /* Phase high16, signed raw value low16. */
uint32_t gs_arp_status(void); /* Optional: running bit31, step bits8–30, note+1 bits0–7. */
float *gs_render(uint32_t frames); /* mono, max 4096 frames */
#ifdef __cplusplus
}
#endif
