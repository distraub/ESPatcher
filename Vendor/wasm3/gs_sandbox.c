#include "gs_sandbox.h"
#include "m3_env.h"
void gs_m3_budget(IM3Runtime runtime, uint64_t fuel) { runtime->gsFuel = fuel; }
void gs_m3_limit(IM3Runtime runtime, uint32_t bytes) { runtime->memoryLimit = bytes; }
