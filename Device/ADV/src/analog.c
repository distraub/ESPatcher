/* Native build of the same source used by the Mac .gsm5 module. */
#define INSTRUMENT 0
#define gs_lfo_status analog_lfo_status
#define gs_arp_status analog_arp_status
#define gs_voice_limit analog_voice_limit
#define gs_active_voices analog_active_voices
#define gs_version analog_version
#define gs_id analog_id
#define gs_init analog_init
#define gs_note analog_note
#define gs_parameter analog_parameter
#define gs_panic analog_panic
#define gs_render analog_render
#include "../../../Firmware/instrument.c"
