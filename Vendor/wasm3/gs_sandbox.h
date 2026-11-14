/* GlitchSynth additions; separate from wasm3 internal structure layouts. */
#pragma once
#include "wasm3.h"
#ifdef __cplusplus
extern "C" {
#endif
void gs_m3_budget(IM3Runtime runtime, uint64_t fuel);
void gs_m3_limit(IM3Runtime runtime, uint32_t bytes);
#ifdef __cplusplus
}
#endif
