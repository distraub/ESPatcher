# MIDI and MIDI Learn

## First implementation

The standalone Mac/iOS app connects CoreMIDI sources automatically and exposes an
ESPatcher virtual MIDI destination. AUv3 receives routed MIDI from its host; it
does not open the same external sources again. Browser users open Firmware… and
choose **Enable MIDI input**. This requests Web MIDI access without SysEx. If the
browser does not expose Web MIDI, the normal computer keys and pads still work.

Notes and velocity are accepted on all channels. Note-on with velocity zero is
note-off. CC120/123 panic; Start/Continue start the arp and Stop stops it. External
24-PPQN clock following, pitch bend, sustain, program change, MIDI output and
physical ADV MIDI drivers are not implemented in this pass. Native incoming MIDI
notes go directly to the engine; the Apple editor's chord mode currently applies
to its note keys/pads. Browser incoming notes also use its chord mode.

## Learn a parameter

1. Open **Firmware… → MIDI & Learn**.
2. Select a parameter, then **Learn next CC**.
3. Move the controller. The first allowed CC binds its exact channel/number
   without changing the sound. Later messages control the parameter.
4. Cancel ends learning; Forget removes that parameter's assignment.

Mappings target stable parameter addresses, not whichever screen or knob happens
to be active. Switching screens does not retarget them. The current reference
engines share a profile and addresses. Arbitrary engine-owned schemas will need
an engine/profile namespace before third-party instrument support broadens.

Sixteen mappings are supported. A channel/CC has one destination and a parameter
has one source; relearning replaces conflicts without evicting unrelated maps.
CC 0, 6, 32, 38, 64, 96–101 and 120–127 are reserved. Other ordinary 7-bit absolute
CCs map across the actual parameter range, logarithmically where appropriate;
discrete parameters are rounded. Relative encoders, 14-bit CC/NRPN and pickup
are future work. After learning, an absolute controller can jump to its current
position on the next message. No incoming MIDI is echoed out.

Standalone mappings persist in application preferences. AU mappings are per
instance and included in `fullState` under `espatcher.midi.v1`. Older sessions
without that field restore empty maps. Browser maps persist in localStorage on
that browser/origin; they are not shared with the native app or other computers.

## Timing and ownership

Standalone CoreMIDI callbacks enqueue bounded MIDI events. Input producers use a
control-side lock; the render callback reads a fixed ring without locking or
allocating. Overflow drops the pending MIDI queue and requests panic. Standalone
MIDI is adopted at the next audio block; timestamp conversion is not implemented.
AU host render events preserve their event offsets, as existing parameter events
do. Learn capture and routing use fixed atomic map cells; UI polling reads small
snapshots and never clocks sound. Browser events enter the AudioWorklet through
messages and take effect on an audio block boundary.

A physical controller, third-party AU host and browser device permission/routing
check are still required on the user's setup. Automated coverage exercises the
actual AU render path and the browser router, not a claim of hardware validation.

## References

GlitchDrums-FM1's [MIDI behavior guide](https://github.com/distraub/glitchdrums-fm1/blob/main/docs/MIDI.md)
and [RemoteFM1's MIDI workflow](https://github.com/Quixotic7/RemoteFM1IOS#midi-controllers-midi-learn-and-the-omx-27)
informed the interaction. This is an independent ESPatcher implementation; no
GPL/MPL MIDI mapping source was copied into the MIT code.
