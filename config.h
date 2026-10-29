// =====================================================================================
//  config.h  -  pins, polarity, tunables, debug flags, and shared types
// =====================================================================================
//
//  Everything you might want to change in the field lives here. The other tabs only
//  hold behavior.
//
//  Why this is a .h file and not another .ino tab:
//    The Arduino build joins every .ino tab into one file and writes function
//    prototypes near the top of it. If a prototype names a struct (for example
//    `void serviceBend(Bend &b, int idx)`) before that struct is defined, the build
//    fails. The main tab #includes this header on its very first line, so all of these
//    types and constants exist before any generated prototype.
//
// =====================================================================================

#pragma once
#include <Arduino.h>

// -------------------------------------------------------------------------------------
//  Pin map (from the manufactured Bend Platform v2 PCB)
// -------------------------------------------------------------------------------------
//  Every button is wired between its pin and ground, and the pin uses INPUT_PULLUP.
//  That makes a button read LOW when pressed and HIGH when released.
//
//  Signal path for each bend:
//    BTN-n (button) -> XIAO input -> XIAO output -> Qn gate (AO3400A)
//                       -> CD4066 control pin -> CD4066 switch -> BND-n (the circuit bend)
//
//  Wiring diagram (Bend Platform v2 PCB -> the device being bent):
//
//    BUTTONS (each one: pin -> button -> GND, pulled up internally)
//
//      D9 ----o MODE o---- GND
//      D2 ----o BTN-1 o--- GND
//      D3 ----o BTN-2 o--- GND
//
//    LED HACK (Q1) - just ONE wire to the device
//
//                         Q1 (AO3400A)
//      D1 -------------- gate
//                        drain --- [LED- pad] ======> device's LED+ (anode)
//                        source -- platform GND (on the PCB)
//
//      Only that single wire goes to the device. The GND pad next to LED- is left
//      unconnected. When Q1 turns on it pulls the LED's anode to ground, so the LED
//      goes dark. When Q1 is off the LED behaves normally.
//
//    BENDS (Q2/Q3 -> CD4066 -> the two bend points)
//
//                    10k to 9 V
//                        |
//      D5 --- Q3 ---- CD4066 control A      CD4066 switch A ===> BND-1 pads (bend 1)
//      D4 --- Q2 ---- CD4066 control B      CD4066 switch B ===> BND-2 pads (bend 2)
//                        |
//                    10k to 9 V
//
//      Each Q pulls its CD4066 control LOW when its gate is HIGH (see BEND_ON below).
//      The CD4066 needs the device to share ground with the platform's GND pad, and the
//      bend-point signals must stay between 0 V and 9 V.
//
//    POWER
//
//      9 V battery -> SS14 diode -> L78M05 -> XIAO 5V pin.
//      Don't plug in USB while the battery is connected.
// -------------------------------------------------------------------------------------
const uint8_t MAIN_LED  = 1;   // D1 -> Q1 gate ("INTRPT"). Q1 shunts the device's main LED.
const uint8_t MODE_BTN  = 9;   // D9 <- MODE button. Steps through the four bend modes.

const uint8_t BTN1_PIN  = 2;   // D2 <- BTN-1 (bend 1)
const uint8_t BTN2_PIN  = 3;   // D3 <- BTN-2 (bend 2)
const uint8_t Q3_GATE   = 5;   // D5 -> Q3 -> CD4066 switch A -> BND-1 (bend 1)
const uint8_t Q2_GATE   = 4;   // D4 -> Q2 -> CD4066 switch B -> BND-2 (bend 2)
                               // The Q numbers cross over (bend 1 is on Q3). That matches
                               // the PCB, so don't "fix" it.

const int NUM_BENDS = 2;       // how many bend channels the board has

// -------------------------------------------------------------------------------------
//  Bend gate polarity
// -------------------------------------------------------------------------------------
//  v2 board: a HIGH gate turns the AO3400A on. It pulls the CD4066 control pin low, which
//            OPENS that switch and cuts the bend in. So on v2 a bend is ACTIVE-LOW.
//  v1 board: a 2N7000 sat directly across the bend, so a HIGH gate meant the bend was ON.
//
//  Set BOARD_V2 to 0 to build for the old v1 board. Other code never writes HIGH or LOW
//  to a bend directly. It always uses BEND_ON or BEND_OFF.
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
//  Q1's drain (the LED- pad) is wired to the device's LED+ (anode, after its resistor).
//  That one wire is the whole LED hack; the GND pad next to it isn't connected.
//    Q1 on  (gate HIGH) -> the anode is pulled to ground, so the LED goes DARK
//    Q1 off (gate LOW)  -> Q1 is out of the way and the LED behaves normally (lit when
//                          the device's own switch has it on)
//
//  After boot, D1 is driven by a fast PWM timer (led.ino), so the LED can sit anywhere
//  between dark and full. The more of each PWM cycle Q1 spends ON, the dimmer the LED.
//  LED_IDLE is only the plain digital level written at power-up, before the PWM starts.
// -------------------------------------------------------------------------------------
const uint8_t LED_IDLE = LOW;   // Q1 off = LED lit, until ledPwmBegin() takes over

// -------------------------------------------------------------------------------------
//  Main LED animation (led.ino)
// -------------------------------------------------------------------------------------
//  Brightness levels are 0.0 (dark) to 1.0 (full). They're "how bright it looks", and
//  LED_GAMMA turns them into PWM duty so 0.5 actually looks about half as bright.
//
//    waiting for a press   LED_IDLE_LEVEL
//    momentary / latching  LED_ACTIVE_LEVEL while any bend is engaged
//    the two trem modes    a soft pulse up to LED_ACTIVE_LEVEL once per stutter cycle,
//                          TREM_PULSE_WIDTH of the cycle wide, with sine-rounded edges
//    mode blink            dark dips (these win over everything else)
// -------------------------------------------------------------------------------------
const float LED_IDLE_LEVEL   = 0.5;   // Brightness while waiting for a press. Experiment:
                                      // lower = dimmer, so the "active" jump stands out more.
const float LED_ACTIVE_LEVEL = 1.0;   // Brightness while a bend is engaged / pulse peak.
const float TREM_PULSE_WIDTH = 0.25;  // Trem pulse width, as a fraction of one full
                                      // on+off stutter cycle (0.25 = 25%). Wider = softer,
                                      // more "breathing"; narrower = a sharper blip.
const float LED_GAMMA        = 2.2;   // Perceived-brightness correction. 1.0 = none
                                      // (linear duty). Higher makes the low end dimmer.
const uint32_t LED_PWM_TOP   = 2047;  // PWM counter top. The timer counts 48 MHz ticks
                                      // from 0 to TOP, so the frequency is
                                      // 48 MHz / (TOP + 1) = about 23.4 kHz: above hearing,
                                      // so no whine. It also gives 2048 brightness steps.
                                      // Keep TOP at 2399 or less (= 20 kHz or more). A
                                      // bigger TOP lowers the frequency into the audio band.

// -------------------------------------------------------------------------------------
//  Tunables (all times in milliseconds)
// -------------------------------------------------------------------------------------
const unsigned long DEBOUNCE_MS    = 50;   // How long a button reading must stay unchanged
                                           // before we trust it. Raise this if a press
                                           // ever registers twice. Lower it for a snappier
                                           // feel.
const unsigned long BLINK_PULSE_MS = 223;  // How long the LED stays dark for each blink.
const unsigned long BLINK_GAP_MS   = 223;  // How long the LED is back on between blinks.
                                           // Both together set how fast the mode number
                                           // gets blinked out.
const unsigned long TREM_MS        = 60;   // Momentary-trem: the bend flips on/off every
                                           // TREM_MS, so a full on+off cycle takes
                                           // 2 x TREM_MS (about 8 Hz). Lower = faster.
const unsigned long LATCH_TREM_MS  = 100;  // Latching-trem: the same idea, a bit slower
                                           // (about 5 Hz).

// -------------------------------------------------------------------------------------
//  Debug flags (compile-time, so nothing is left in the build when set to 0)
// -------------------------------------------------------------------------------------
#define LOG 1        // 1 = print an event log over Serial at 9600 baud
                     //     (for example "MODE -> mode 3 (latching)").

// -------------------------------------------------------------------------------------
//  Bend modes
// -------------------------------------------------------------------------------------
//  The modes start at 1 so the mode number is also how many times the LED blinks.
//  Pressing MODE steps 1 -> 2 -> 3 -> 4 -> 1 ...
// -------------------------------------------------------------------------------------
enum Mode {
  MOMENTARY = 1,     // the bend is on only while its button is held
  MOMENTARY_TREM,    // the bend stutters on/off while its button is held
  LATCHING,          // each press toggles the bend on or off
  LATCHING_TREM,     // each press toggles a stuttering bend on or off
};
const int NUM_MODES = LATCHING_TREM;

// Names used in the serial log, indexed by mode number. Index 0 is unused because the
// modes start at 1.
const char* const MODE_NAMES[] = { "", "momentary", "momentary-trem", "latching", "latching-trem" };

// -------------------------------------------------------------------------------------
//  Types
// -------------------------------------------------------------------------------------

// One debounced button. The bend buttons and the MODE button all use this.
// pressed() in buttons.ino updates it.
struct Button {
  uint8_t pin;               // input pin (INPUT_PULLUP)
  int lastRead;              // last raw digitalRead(), used to spot changes
  unsigned long lastChange;  // millis() when the raw reading last changed
  bool down;                 // debounced state: true = pressed
};

// One circuit bend channel: its button, the gate pin it drives, and its latch state.
// bends.ino updates it. (The stutter timing is shared by all bends, so it lives in the
// main tab as tremStart / tremRunning rather than in here.)
struct Bend {
  Button button;             // the button for this bend
  uint8_t gate;              // XIAO output that drives this bend's Q gate
  bool latched;              // latching modes: is the bend toggled on?
};
