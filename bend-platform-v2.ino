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
//  Bend modes (a short MODE press steps 1 -> 2 -> 3 -> 4 -> 5 -> 1; the last one is kept in
//  flash between power-ups):
//    1 momentary       bend is on while the button is held
//    2 momentary-trem  bend stutters while the button is held
//    3 latching        each press toggles the bend
//    4 latching-trem   each press toggles a stuttering bend
//    5 sequencer       plays a pattern across both bends (sequence.ino). BTN-1 starts
//                      and stops it, BTN-2 plays it while held.
//
//  The LED:
//    waiting           dim (half brightness)
//    modes 1, 3, 5     full while either bend is on
//    modes 2 and 4     a soft pulse in time with the stutter
//    MODE press        N dark blinks for mode N (also at power-up). Blinks override the
//                      animation; between blinks the animation shows.
//
//  Speed mode (long-press MODE in mode 2, 4 or 5 to enter, again to leave; the LED
//  ramps up twice going in, down twice coming out):
//    The LED pulses at the stutter speed. BTN-1 slows it down, BTN-2 speeds it up. The
//    bends keep running, so you hear it; presses don't toggle a latch. The new speed applies to modes 2, 4 and 5, and is kept in flash
//    between power-ups.
//
//  Each loop() pass runs in five stages:
//    1 read      sample every button once
//    2 decide    MODE: change the mode, enter/leave speed mode
//    3 bends     drive the bend gates (or set the speed)
//    4 LED       blinks, sweeps and the bend animation
//    5 upkeep    save settings, feed the watchdog
//
//  Tabs:
//    config.h      pins, polarity, timings, debug flag, types (settings live here)
//    buttons.ino   reads the buttons (debounced) + the elapsed() timer
//    bends.ino     bend modes 1-4
//    sequence.ino  bend mode 5: the step sequencer (a template for your own mode)
//    mode.ino      the MODE button: short and long press
//    speed.ino     speed mode
//    flash.ino     saves settings (speed and mode) between power-ups
//    led.ino       LED dimming, blinks and sweeps, bend animation
//    watchdog.ino  resets the board if the sketch hangs
//
// =====================================================================================

#include "config.h"

// -------------------------------------------------------------------------------------
//  Global state (shared across tabs)
// -------------------------------------------------------------------------------------

// Fields: { { button pin, lastRead, lastChange, down, downSince, edge }, gate pin, latched, on, stuttering }
// lastRead starts HIGH: a released button reads HIGH with INPUT_PULLUP.
Bend bends[NUM_BENDS] = {
  { { BTN1_PIN, HIGH, 0, false, 0, BTN_NONE }, BEND1_GATE, false, false, false },  // BTN-1 -> Q3 -> CD4066 A -> BND-1
  { { BTN2_PIN, HIGH, 0, false, 0, BTN_NONE }, BEND2_GATE, false, false, false },  // BTN-2 -> Q2 -> CD4066 B -> BND-2
};

Button modeBtn = { MODE_PIN, HIGH, 0, false, 0, BTN_NONE };
int mode = MOMENTARY;                            // current bend mode (1..NUM_MODES)

// One stutter clock shared by the bends and the LED, so they stay in sync.
unsigned long tremStart = 0;     // millis() when the current stutter began
bool tremRunning = false;        // was any bend following the clock on the previous loop?
float tremSpeed = 1.0;           // clock speed multiplier, set in speed mode
int speedSteps = 0;              // tremSpeed in steps from normal: - slower, + faster

bool speedMode = false;          // true while in speed mode (speed.ino)

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
  digitalWrite(LED_GATE, LED_GATE_AT_BOOT);  // Q1 off: LED behaves normally
  pinMode(LED_GATE, OUTPUT);
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
  restoreSettings();      // the last saved speed and mode (flash.ino)
  logMode("mode ");

  startModeBlink(mode);   // blink the starting mode: shows the board is alive

  watchdogBegin();        // from here on, loop() must keep feeding it (watchdog.ino)
}

// -------------------------------------------------------------------------------------
//  loop()  -  runs forever
// -------------------------------------------------------------------------------------
void loop() {
  // 1. Read: sample every button once
  readButtons();        // BTN-1, BTN-2, MODE -> each button's .edge (buttons.ino)

  // 2. Decide: mode and speed logic
  serviceMode();        // MODE: next mode, or enter/leave speed mode (mode.ino)

  // 3. Drive the bends
  serviceBends();       // BTN-1 / BTN-2: set the gates, or set the speed (bends.ino)

  // 4. Drive the LED (sees this loop's bends)
  serviceBlink();       // advance the blink timing (led.ino)
  serviceLed();         // set the LED brightness (led.ino)

  // 5. Housekeeping
  serviceSave();        // save the mode once it's settled (flash.ino)
  watchdogFeed();       // still running: restart the watchdog countdown (watchdog.ino)
}
