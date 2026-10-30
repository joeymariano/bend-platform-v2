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
const uint8_t LED_GATE   = 1;   // D1 -> Q1 gate (dims the device's LED)
const uint8_t MODE_PIN   = 9;   // D9 <- MODE button

const uint8_t BTN1_PIN   = 2;   // D2 <- BTN-1 (bend 1)
const uint8_t BTN2_PIN   = 3;   // D3 <- BTN-2 (bend 2)
const uint8_t BEND1_GATE = 5;   // D5 -> Q3 -> CD4066 A -> BND-1 (Q3, not Q2: matches the PCB)
const uint8_t BEND2_GATE = 4;   // D4 -> Q2 -> CD4066 B -> BND-2

const int NUM_BENDS = 2;

// -------------------------------------------------------------------------------------
//  Bend polarity
// -------------------------------------------------------------------------------------
//  gate HIGH -> Q on  -> CD4066 control pulled low -> switch OPEN   -> bend off
//  gate LOW  -> Q off -> control pulled to 9 V     -> switch CLOSED -> bend on
//
//  So bends are active-LOW. Always write BEND_ON / BEND_OFF, never HIGH/LOW.
// -------------------------------------------------------------------------------------
const uint8_t BEND_ON = LOW, BEND_OFF = HIGH;

// -------------------------------------------------------------------------------------
//  Main LED polarity (Q1)
// -------------------------------------------------------------------------------------
//  Q1 on = LED dark. Q1 off = LED normal (lit, if the device has it on).
//  After setup(), a PWM timer drives Q1 (led.ino): more on-time = dimmer LED.
//  LED_GATE_AT_BOOT is only the level written at power-up, before the PWM starts.
// -------------------------------------------------------------------------------------
const uint8_t LED_GATE_AT_BOOT = LOW;   // Q1 off = LED lit

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
static_assert(LED_PWM_TOP <= 2399, "LED_PWM_TOP above 2399 puts the PWM below 20 kHz (audible)");

// -------------------------------------------------------------------------------------
//  Timings (milliseconds)
// -------------------------------------------------------------------------------------
const unsigned long DEBOUNCE_MS    = 50;   // raise if a press counts twice
const unsigned long LONG_PRESS_MS  = 1000; // hold MODE this long, then let go: speed mode in/out
const unsigned long BLINK_PULSE_MS = 223;  // dark time per mode blink
const unsigned long BLINK_GAP_MS   = 223;  // lit time between mode blinks
const unsigned long FLASH_MS       = 40;   // dark and lit time per fast flash
const int           FLASH_COUNT    = 5;    // fast flashes on entering/leaving speed mode
const unsigned long TREM_MS        = 60;   // momentary-trem: on 60, off 60 (about 8 Hz)
const unsigned long LATCH_TREM_MS  = 100;  // latching-trem: on 100, off 100 (5 Hz)
                                           // (both at normal speed; see Speed mode below)

// -------------------------------------------------------------------------------------
//  Speed mode (speed.ino)
// -------------------------------------------------------------------------------------
//  Each BTN-1 / BTN-2 press changes the stutter speed by SPEED_STEP, at most
//  SPEED_MAX_STEPS each way. 1.1^14 = 3.8, so the range is about 0.26x to 3.8x.
// -------------------------------------------------------------------------------------
const float SPEED_STEP      = 1.1;   // 10% per press
const int   SPEED_MAX_STEPS = 14;

// -------------------------------------------------------------------------------------
//  Saved settings (flash.ino)
// -------------------------------------------------------------------------------------
//  Kept in flash between power-ups. Uploading a new sketch erases them.
//  Bump SETTINGS_VERSION whenever the Settings type changes, so old saves are ignored.
// -------------------------------------------------------------------------------------
const uint32_t SETTINGS_VERSION = 1;

// -------------------------------------------------------------------------------------
//  Watchdog (watchdog.ino)
// -------------------------------------------------------------------------------------
//  If loop() stops for this long, the board resets itself. Counts 1,024 Hz ticks:
//  WDT_CONFIG_PER_1K_Val = 1 s, _2K = 2 s, _4K = 4 s, _8K = 8 s, _16K = 16 s.
// -------------------------------------------------------------------------------------
const uint8_t WDT_TIMEOUT = WDT_CONFIG_PER_2K_Val;   // about 2 s

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
static_assert(sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]) == NUM_MODES + 1,
              "MODE_NAMES needs one name per mode, plus the unused \"\" at index 0");

// -------------------------------------------------------------------------------------
//  Types
// -------------------------------------------------------------------------------------

// A debounced button. Updated by readButton() (buttons.ino).
struct Button {
  uint8_t pin;               // input pin
  int lastRead;              // last raw reading
  unsigned long lastChange;  // millis() when the raw reading last changed
  bool down;                 // debounced: true = pressed
  unsigned long downSince;   // millis() when the current press began
};

// What readButton() saw this loop.
enum ButtonEdge { BTN_NONE, BTN_PRESS, BTN_RELEASE };

// One bend: its button, its gate pin, and what it's doing. (The stutter clock is
// shared by all bends, so it lives in the main tab.)
struct Bend {
  Button button;
  uint8_t gate;              // output pin to this bend's Q gate
  bool latched;              // latching modes: toggled on?
  bool on;                   // is the gate on this loop? (set by setBend)
  bool stuttering;           // is it stuttering this loop? (set by setBend)
};

// What's kept in flash between power-ups (flash.ino). 32-bit fields only: flash is
// written a 32-bit word at a time.
struct Settings {
  int32_t speedSteps;        // speed mode: steps from normal speed
};
static_assert(sizeof(Settings) % 4 == 0, "Settings must be whole 32-bit words");
