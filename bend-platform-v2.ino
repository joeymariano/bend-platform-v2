// =====================================================================================
//  Bend Platform v2  -  main tab
// =====================================================================================
//
//  Adds buttons to a device (a fuzz pedal, for example) that switch two circuit "bends"
//  (deliberate shorts or cuts) in and out. Runs on a Seeeduino XIAO SAMD21.
//
//  Hardware:
//    MODE button     picks the bend mode
//    BTN-1, BTN-2    one button per bend
//    Q1              dims or darkens the device's own LED, used as a status light
//    Q2, Q3          drive the CD4066 switches that make the bends
//
//  Bend modes (MODE steps 1 -> 2 -> 3 -> 4 -> 1):
//    1 momentary       bend is on while the button is held
//    2 momentary-trem  bend stutters while the button is held
//    3 latching        each press toggles the bend
//    4 latching-trem   each press toggles a stuttering bend
//
//  The LED:
//    waiting           dim (half brightness)
//    modes 1 and 3     full while either bend is on
//    modes 2 and 4     a soft pulse in time with the stutter
//    MODE press        N dark blinks for mode N (also at power-up). Blinks override the
//                      animation; between blinks the animation shows.
//
//  Tabs:
//    config.h      pins, polarity, timings, debug flag, types (settings live here)
//    buttons.ino   button debouncing + the elapsed() timer
//    bends.ino     the four bend modes
//    mode.ino      the MODE button
//    led.ino       LED dimming, mode blink, bend animation
//    watchdog.ino  resets the board if the sketch hangs
//
// =====================================================================================

#include "config.h"

// -------------------------------------------------------------------------------------
//  Global state (shared across tabs)
// -------------------------------------------------------------------------------------

// Fields: { { button pin, lastRead, lastChange, down }, gate pin, latched, on, stuttering }
// lastRead starts HIGH: a released button reads HIGH with INPUT_PULLUP.
Bend bends[NUM_BENDS] = {
  { { BTN1_PIN, HIGH, 0, false }, Q3_GATE, false, false, false },  // BTN-1 -> Q3 -> CD4066 A -> BND-1
  { { BTN2_PIN, HIGH, 0, false }, Q2_GATE, false, false, false },  // BTN-2 -> Q2 -> CD4066 B -> BND-2
};

Button modeBtn = { MODE_BTN, HIGH, 0, false };
int mode = MOMENTARY;                            // current bend mode (1..NUM_MODES)

// One stutter clock shared by the bends and the LED, so they stay in sync.
unsigned long tremStart = 0;     // millis() when the current stutter began
bool tremRunning = false;        // was any bend stuttering on the previous loop?

// What the bends are doing this loop. Rebuilt by serviceBends(), read by led.ino.
bool anyEngaged = false;         // at least one bend is on
bool anyStuttering = false;      // at least one bend is stuttering

// -------------------------------------------------------------------------------------
//  setup()  -  runs once at power-up
// -------------------------------------------------------------------------------------
void setup() {
  // The gates float until driven. Writing the level before pinMode(OUTPUT) means each
  // pin starts out at its safe level, with no glitch.
  for (int i = 0; i < NUM_BENDS; i++) {
    digitalWrite(bends[i].gate, BEND_OFF);
    pinMode(bends[i].gate, OUTPUT);
  }
  digitalWrite(MAIN_LED, LED_IDLE);          // Q1 off: LED behaves normally
  pinMode(MAIN_LED, OUTPUT);
  ledPwmBegin();                             // the PWM timer takes over D1 (led.ino)

#if LOG
  Serial.begin(9600);                        // event log
#endif

  // Buttons connect to ground: HIGH = released, LOW = pressed.
  for (int i = 0; i < NUM_BENDS; i++) pinMode(bends[i].button.pin, INPUT_PULLUP);
  pinMode(modeBtn.pin, INPUT_PULLUP);

#if LOG
  Serial.println("bend-platform-v2 ready");
#endif
  logResetCause();
  logMode("mode ");

  startModeBlink(mode);   // blink the starting mode: shows the board is alive

  watchdogBegin();        // from here on, loop() must keep feeding it (watchdog.ino)
}

// -------------------------------------------------------------------------------------
//  loop()  -  runs forever
// -------------------------------------------------------------------------------------
void loop() {
  // Bends first, so the LED sees this loop's state.
  serviceBends();       // read BTN-1 / BTN-2, drive the bends (bends.ino)
  serviceMode();        // read MODE, change mode, start the blink (mode.ino)
  serviceModeBlink();   // advance the blink timing (led.ino)
  serviceLed();         // set the LED brightness (led.ino)
  watchdogFeed();       // still running: restart the watchdog countdown (watchdog.ino)
}
