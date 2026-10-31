---
id: sos-requirements
type: requirements
status: draft
owner: KhoaDD
summary: SOS local alarm requirements, starting with the common SOS pattern on the buzzer and the vibrator (findings F2 and F3 of the 2026-10-29 v2 run).
---

# SOS: requirements (local alarm)

The trigger, cancel and wire format requirements are still the variant specs listed in
[README.md](README.md). This file adds the local alarm, ruled by the leader on 2026-10-31: one
common SOS pattern for every output that can signal, sparse enough to last.

## REQ-SOS-ALM-01: Common SOS pattern

1. WHILE an SOS is active (button, gesture or fall auto-SOS), the system SHALL drive every local
   alarm output the variant has (buzzer, vibration motor, SOS LED) with the same Morse "SOS"
   pattern: three short, three long, three short.
2. The pattern SHALL repeat once every 5 seconds, with the outputs off for the rest of each period.
3. Timing: one unit is 120 ms; a short mark is 1 unit, a long mark 3 units, the gap inside a letter
   1 unit, the gap between letters 3 units. The pattern lasts 27 units (3.24 s) and is followed by
   1.76 s off.
4. WHEN the SOS is cancelled, THEN every output SHALL turn off within one pattern unit.
5. The trigger confirmation (three short vibration pulses) and the cancel confirmation (two long
   pulses) SHALL keep their own timing; the SOS pattern takes over the vibration motor once the
   trigger confirmation ends.

## REQ-SOS-ALM-02: The alarm survives stock sounds

1. WHEN stock code plays a tone on the buzzer pin during an SOS (button feedback, message
   notification), THEN the SOS pattern SHALL be audible again no later than its next mark.
   Root cause of the old silence: Arduino-ESP32 2.0.17 `tone()` reconfigures LEDC timer 0 to 10 bit
   at the note frequency and detaches the pin when the note ends (`cores/esp32/Tone.cpp:33-47`,
   `esp32-hal-ledc.c` `ledcWriteTone`), while `BuzzerManager` only wrote the duty (finding F2).
2. The system SHALL therefore set up the LEDC channel and attach the buzzer pin again at the start of
   every mark.

## REQ-SOS-ALM-03: The fall pre-alarm stays distinct

1. WHILE a fall pre-alarm counts down, the system SHALL keep the pre-alarm signal (a 200 ms beep and
   pulse every second), so a person can tell "about to send" from "sent".
