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
//  Mode indication:
//    Pressing MODE makes the device's main LED (normally lit) blink dark N times, where N
//    is the new mode number (1..4). It also blinks once at power-up.
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
//    led.ino               non-blocking mode blink on the main LED + the Q1 bench test
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
//   { { button pin, lastRead, lastChange, down }, gate pin, latched, tremPhase, lastTrem }
// lastRead starts HIGH because a released button reads HIGH with INPUT_PULLUP.
Bend bends[NUM_BENDS] = {
  { { BTN1_PIN, HIGH, 0, false }, Q3_GATE, false, false, 0 },  // BTN-1 -> Q3 -> CD4066 A -> BND-1
  { { BTN2_PIN, HIGH, 0, false }, Q2_GATE, false, false, 0 },  // BTN-2 -> Q2 -> CD4066 B -> BND-2
};

Button modeBtn = { MODE_BTN, HIGH, 0, false };  // the MODE button
int mode = MOMENTARY;                            // current bend mode (1..NUM_MODES)

// Mode-blink sequencer state, used by led.ino.
int blinkRemaining = 0;          // blinks still to finish (counts the one in progress)
bool blinkPulsing = false;       // true while the LED is held dark for a blink
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
#if T1_TEST
  t1BlinkTest();   // bench mode: only toggle Q1. Set T1_TEST 0 in config.h to play.
  return;
#endif

  // Each call does a small slice of work and returns right away (see led.ino / bends.ino).
  for (int i = 0; i < NUM_BENDS; i++) serviceBend(bends[i], i);  // read BTN-n, drive bend n
  serviceMode();        // read MODE, step to the next mode, start the blink
  serviceModeBlink();   // move the LED blink sequence along
}
