#pragma once

#if defined(ARCH_PORTDUINO) && defined(TREKLINK_SCREENCAP)

/**
 * TrekLink screen capture (tools/screencap/README.md).
 *
 * The native capture build replaces each TrekLink button with a simulated pin and reads commands on stdin, one per
 * line, so a script can press the buttons the way a person does and save the screen the firmware drew:
 *
 * - press <button> <ms>: hold a button for <ms> milliseconds, then release it
 * - shot <path>: save the current screen as a PBM file, answered by "SCREENCAP shot <path>"
 * - heading <degrees>: point the simulated compass (v2 only, which has the ICM-20948)
 * - quit: exit
 *
 * Buttons: v2 "up", "down", "menu", "sos"; v3 and v4 "user" (the middle user button).
 */

/** Highest simulated GPIO the capture build uses; portduino must allocate at least this many pins. */
#define TREKLINK_SCREENCAP_MAX_PIN 8

/** Binds the simulated button pins. Call right after gpioInit(). */
void screenCapBindButtons();

/** Starts the stdin command thread. Call once the screen exists. */
void screenCapStart();

#endif
