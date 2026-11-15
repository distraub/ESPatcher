/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Avoid the Arduino lifecycle/time names; no changes to firmware DSP. */
#if GD_PORT_NATIVE
#define setup gdfw_setup
#define loop gdfw_loop
#define millis gdfw_millis
#define micros gdfw_micros
#endif
