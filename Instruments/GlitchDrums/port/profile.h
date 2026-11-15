/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#if GD_PORT_PROFILE
extern uint32_t gdp_clock_us(void);
volatile uint32_t gd_profile_active=1,gd_profile_requested=1;
static uint32_t gdp_work[11],gdp_peaks[13];
uint32_t gd_profile_now(void){return gdp_clock_us();}
void gd_profile_add(unsigned stage,uint32_t elapsed){if(stage<11)gdp_work[stage]+=elapsed;}
void gd_profile_read(uint32_t out[13]){for(unsigned i=0;i<13;i++)out[i]=gdp_peaks[i];}
static void gdp_profile_begin(void){for(unsigned i=0;i<11;i++)gdp_work[i]=0;}
static void gdp_profile_end(uint32_t elapsed){uint32_t sum=0;for(unsigned i=0;i<11;i++){sum+=gdp_work[i];if(gdp_work[i]>gdp_peaks[i])gdp_peaks[i]=gdp_work[i];}uint32_t other=elapsed>sum?elapsed-sum:0;if(other>gdp_peaks[11])gdp_peaks[11]=other;if(elapsed>gdp_peaks[12])gdp_peaks[12]=elapsed;}
#else
static void gdp_profile_begin(void){}
static void gdp_profile_end(uint32_t elapsed){(void)elapsed;}
#endif
