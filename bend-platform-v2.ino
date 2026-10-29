// =====================================================================================
//  Bend Platform v2  -  main tab
// =====================================================================================
//
//  Adds physical buttons to a device (a guitar fuzz/distortion pedal, for
//  example). The buttons open and close circuit "bends" (deliberate shorts or cuts in the
//  circuit) in a musical way. A Seeeduino XIAO SAMD21 on the Bend Platform v2 PCB runs it.
//
//  Hardware at a glance:
//    3 buttons       MODE (picks the bend behavior) + BTN-1 and BTN-2 (one per bend)
//    3 MOSFETs       Q1 cuts out the device's main LED, to show the mode
//                    Q2 and Q3 drive the CD4066 analog switches that open/close the bends
//
//  The main LED (via Q1, dimmed with an ultrasonic PWM):
//    - Waiting for a press: sits dim (about half brightness).
//    - Momentary / latching: goes full brightness while either bend is engaged.
//    - The two trem modes: pulses softly, in time with the stutter, while a bend stutters.
//    - Pressing MODE: blinks dark N times, where N is the new mode number (1..4). It also
//      blinks once at power-up. The blink wins over the animation while it runs.
//
//  Bend modes (cycled by MODE):
//    1 momentary       the bend is on while the button is held
//    2 momentary-trem  the bend stutters while the button is held
//    3 latching        a press toggles the bend on or off
//    4 latching-trem   a press toggles a stuttering bend on or off
//
//  Tab map (the Arduino IDE shows each file as a tab):
//    bend-platform-v2.ino  this file: global state, setup(), loop()
//    config.h              pins, polarity, timings, debug flags, types. Edit settings here.
//    buttons.ino           debounced button reading + the elapsed() timer helper
//    bends.ino             drives the bend switches in each of the four modes
//    mode.ino              MODE button: steps through the modes
//    led.ino               main LED: ultrasonic PWM, mode blink, bend animation
//
//  How the tabs fit together:
//    Arduino builds this tab first, then the other .ino tabs in alphabetical order, and
//    joins them into one file. Globals defined here are therefore visible in every other
//    tab. Types live in config.h (see the note at the top of that file for why).
//
//  Nothing here ever blocks (no delay()). loop() runs thousands of times per second and
//  each service function does a little work per pass, so the buttons always respond
//  instantly, even while the LED is blinking or a tremolo is running.
//
// =====================================================================================

#include "config.h"

// -------------------------------------------------------------------------------------
//  Global state (shared across tabs)
// -------------------------------------------------------------------------------------

// The two bend channels. Initializer order follows the Bend / Button structs in config.h:
//   { { button pin, lastRead, lastChange, down }, gate pin, latched }
// lastRead starts HIGH because a released button reads HIGH with INPUT_PULLUP.
Bend bends[NUM_BENDS] = {
  { { BTN1_PIN, HIGH, 0, false }, Q3_GATE, false },  // BTN-1 -> Q3 -> CD4066 A -> BND-1
  { { BTN2_PIN, HIGH, 0, false }, Q2_GATE, false },  // BTN-2 -> Q2 -> CD4066 B -> BND-2
};

Button modeBtn = { MODE_BTN, HIGH, 0, false };  // the MODE button
int mode = MOMENTARY;                            // current bend mode (1..NUM_MODES)

// Shared stutter clock, used by bends.ino (to flip the bends) and led.ino (to pulse the
// LED in sync). One clock for everything is what keeps them lined up.
unsigned long tremStart = 0;     // millis() when the current stutter began
bool tremRunning = false;        // was any bend stuttering on the previous loop?

// What the bends are doing *this* loop. serviceBends() rebuilds these on every pass, and
// led.ino reads them to choose the LED animation.
bool anyEngaged = false;         // at least one bend gate is on
bool anyStuttering = false;      // at least one bend is in a stutter

// Mode-blink sequencer state, used by led.ino.
int blinkRemaining = 0;          // blinks still to finish (counts the one in progress)
bool blinkPulsing = false;       // true while the LED should be held dark for a blink
unsigned long blinkStamp = 0;    // millis() when the current pulse or gap began

// -------------------------------------------------------------------------------------
//  setup()  -  runs once at power-up / reset
// -------------------------------------------------------------------------------------
void setup() {
  // Outputs first, as early as possible. The gates have no external pull-downs, so they
  // float until we drive them. Writing the level BEFORE pinMode(OUTPUT) means that when
  // the pin starts driving, it already drives the safe level, with no glitch where a
  // bend snaps on or the LED flickers.
  for (int i = 0; i < NUM_BENDS; i++) {
    digitalWrite(bends[i].gate, BEND_OFF);   // both bends start disengaged
    pinMode(bends[i].gate, OUTPUT);
  }
  digitalWrite(MAIN_LED, LED_IDLE);          // LED at rest (Q1 off, LED behaves normally)
  pinMode(MAIN_LED, OUTPUT);
  ledPwmBegin();                             // hand D1 to the ultrasonic PWM timer (led.ino)

  Serial.begin(9600);                        // for the event log (see LOG in config.h)

  // Buttons: each one is wired to ground, and the internal pull-up makes it read
  // HIGH when released and LOW when pressed.
  for (int i = 0; i < NUM_BENDS; i++) pinMode(bends[i].button.pin, INPUT_PULLUP);
  pinMode(modeBtn.pin, INPUT_PULLUP);

#if LOG
  Serial.println("bend-platform-v2 ready");
  Serial.print("mode "); Serial.print(mode);
  Serial.print(" ("); Serial.print(MODE_NAMES[mode]); Serial.println(")");
#endif

  startModeBlink(mode);   // blink the starting mode once so you know the board is alive
}

// -------------------------------------------------------------------------------------
//  loop()  -  runs forever, as fast as it can
// -------------------------------------------------------------------------------------
void loop() {
  // Each call does a small slice of work and returns right away. Order matters a little:
  // the bends run first so the LED sees this loop's engaged / stuttering state.
  serviceBends();       // read BTN-1 / BTN-2, drive both bends (bends.ino)
  serviceMode();        // read MODE, step to the next mode, start the blink (mode.ino)
  serviceModeBlink();   // move the mode-blink sequence along (led.ino)
  serviceLed();         // pick this loop's LED brightness and write it (led.ino)
}
