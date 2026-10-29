// =====================================================================================
//  config.h  -  pins, polarity, settings, debug flag, shared types
// =====================================================================================
//
//  Every setting lives here. The other tabs hold only behavior.
//
//  Why a .h and not an .ino tab: Arduino adds function prototypes at the top of the
//  joined sketch. A prototype that uses a struct (like `void serviceBend(Bend &b, int idx)`)
//  breaks the build if the struct isn't defined yet. The main tab includes this file
//  first, so the types always come first.
//
// =====================================================================================

#pragma once
#include <Arduino.h>

// -------------------------------------------------------------------------------------
//  Pin map (Bend Platform v2 PCB)
// -------------------------------------------------------------------------------------
//
//    BUTTONS (pin -> button -> GND, INPUT_PULLUP: LOW = pressed)
//
//      D9 ----o MODE o---- GND
//      D2 ----o BTN-1 o--- GND
//      D3 ----o BTN-2 o--- GND
//
//    LED HACK (Q1): one wire to the device
//
//                         Q1 (AO3400A)
//      D1 -------------- gate
//                        drain --- [LED- pad] ======> device's LED+ (anode)
//                        source -- platform GND
//
//      Q1 on pulls the LED's anode to ground (dark). Q1 off leaves the LED alone.
//      The GND pad next to LED- stays unconnected.
//
//    BENDS (Q2/Q3 -> CD4066 -> bend points)
//
//                    10k to 9 V
//                        |
//      D5 --- Q3 ---- CD4066 control A      CD4066 switch A ===> BND-1 pads (bend 1)
//      D4 --- Q2 ---- CD4066 control B      CD4066 switch B ===> BND-2 pads (bend 2)
//                        |
//                    10k to 9 V
//
//      The device must share ground with the platform, and the bend signals must stay
//      between 0 V and 9 V.
//
//    POWER: 9 V battery -> SS14 diode -> L78M05 -> XIAO 5V pin.
//           Don't plug in USB while the battery is connected.
// -------------------------------------------------------------------------------------
const uint8_t MAIN_LED  = 1;   // D1 -> Q1 gate (dims the device's LED)
const uint8_t MODE_BTN  = 9;   // D9 <- MODE button

const uint8_t BTN1_PIN  = 2;   // D2 <- BTN-1 (bend 1)
const uint8_t BTN2_PIN  = 3;   // D3 <- BTN-2 (bend 2)
const uint8_t Q3_GATE   = 5;   // D5 -> Q3 -> CD4066 A -> BND-1 (bend 1)
const uint8_t Q2_GATE   = 4;   // D4 -> Q2 -> CD4066 B -> BND-2 (bend 2)
                               // Bend 1 is on Q3, not Q2. That matches the PCB.

const int NUM_BENDS = 2;

// -------------------------------------------------------------------------------------
//  Bend polarity
// -------------------------------------------------------------------------------------
//  v2: gate HIGH -> Q on -> CD4066 control pulled low -> switch OPEN  -> bend off
//      gate LOW  -> Q off -> control pulled to 9 V    -> switch CLOSED -> bend on
//      So v2 bends are active-LOW.
//  v1: a 2N7000 sat across the bend, so gate HIGH = bend on.
//
//  Set BOARD_V2 to 0 for the v1 board. Always write BEND_ON / BEND_OFF, never HIGH/LOW.
// -------------------------------------------------------------------------------------
#define BOARD_V2 1
#if BOARD_V2
const uint8_t BEND_ON = LOW,  BEND_OFF = HIGH;
#else
const uint8_t BEND_ON = HIGH, BEND_OFF = LOW;
#endif

// -------------------------------------------------------------------------------------
//  Main LED polarity (Q1)
// -------------------------------------------------------------------------------------
//  Q1 on = LED dark. Q1 off = LED normal (lit, if the device has it on).
//  After setup(), a PWM timer drives Q1 (led.ino): more on-time = dimmer LED.
//  LED_IDLE is only the level written at power-up, before the PWM starts.
// -------------------------------------------------------------------------------------
const uint8_t LED_IDLE = LOW;   // Q1 off = LED lit

// -------------------------------------------------------------------------------------
//  LED animation (led.ino)
// -------------------------------------------------------------------------------------
//  Brightness runs from 0.0 (dark) to 1.0 (full), as it looks to the eye.
// -------------------------------------------------------------------------------------
const float LED_IDLE_LEVEL   = 0.5;   // waiting. Lower = the active jump stands out more.
const float LED_ACTIVE_LEVEL = 1.0;   // bend on, and the peak of the trem pulse
const float TREM_PULSE_WIDTH = 0.25;  // trem pulse length, as a fraction of a stutter
                                      // cycle. Wider = softer, narrower = sharper.
const float LED_GAMMA        = 2.2;   // brightness correction. 1.0 = none.
const uint32_t LED_PWM_TOP   = 2047;  // PWM frequency = 48 MHz / (TOP + 1) = 23.4 kHz.
                                      // Keep TOP <= 2399 (>= 20 kHz) to stay above hearing.

// -------------------------------------------------------------------------------------
//  Timings (milliseconds)
// -------------------------------------------------------------------------------------
const unsigned long DEBOUNCE_MS    = 50;   // raise if a press counts twice
const unsigned long BLINK_PULSE_MS = 223;  // dark time per mode blink
const unsigned long BLINK_GAP_MS   = 223;  // lit time between mode blinks
const unsigned long TREM_MS        = 60;   // momentary-trem: on 60, off 60 (about 8 Hz)
const unsigned long LATCH_TREM_MS  = 100;  // latching-trem: on 100, off 100 (5 Hz)

// -------------------------------------------------------------------------------------
//  Debug flag
// -------------------------------------------------------------------------------------
#define LOG 1        // 1 = event log over Serial at 9600 baud, e.g. "MODE -> mode 3 (latching)"

// -------------------------------------------------------------------------------------
//  Bend modes
// -------------------------------------------------------------------------------------
//  Numbered from 1, so the mode number is also its blink count.
// -------------------------------------------------------------------------------------
enum Mode {
  MOMENTARY = 1,     // on while held
  MOMENTARY_TREM,    // stutters while held
  LATCHING,          // each press toggles
  LATCHING_TREM,     // each press toggles a stutter
};
const int NUM_MODES = LATCHING_TREM;

// Log names, indexed by mode number (index 0 unused).
const char* const MODE_NAMES[] = { "", "momentary", "momentary-trem", "latching", "latching-trem" };

// -------------------------------------------------------------------------------------
//  Types
// -------------------------------------------------------------------------------------

// A debounced button. Updated by pressed() (buttons.ino).
struct Button {
  uint8_t pin;               // input pin
  int lastRead;              // last raw reading
  unsigned long lastChange;  // millis() when the raw reading last changed
  bool down;                 // debounced: true = pressed
};

// One bend: its button, its gate pin, and what it's doing. (The stutter clock is
// shared by all bends, so it lives in the main tab.)
struct Bend {
  Button button;
  uint8_t gate;              // output pin to this bend's Q gate
  bool latched;              // latching modes: toggled on?
  bool on;                   // is the gate on this loop? (set by serviceBend)
  bool stuttering;           // is it stuttering this loop? (set by serviceBend)
};
