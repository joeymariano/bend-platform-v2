// =====================================================================================
//  led.ino  -  the device's main LED (via Q1): PWM dimming, mode blink, bend animation
// =====================================================================================
//
//  There's no separate status LED. Q1 is wired to the device's own LED+ and can pull it
//  to ground, which darkens it. Switching Q1 on and off very fast (PWM) means it can sit
//  at any brightness in between. This tab turns that into a status display.
//
//  What the LED shows, highest priority first:
//
//    1. Mode blink       dark dips, N of them for mode N (after a MODE press, or at boot)
//    2. Trem modes       while a bend stutters: a soft pulse once per stutter cycle,
//                        locked to the same clock the bends use
//    3. Momentary/latch  full brightness while either bend is engaged
//    4. Waiting          dim (LED_IDLE_LEVEL)
//
//  Mode-blink timeline for mode 3 (# = dark, . = normal brightness):
//
//      ###...###...###      then back to the animation
//      |-| BLINK_PULSE_MS
//         |-| BLINK_GAP_MS
//
//  Trem pulse, one stutter cycle (2 x the mode's stutter interval):
//
//      bend:  |^^^^^^^^^^^^^|_____________|^^^^...     on half, off half
//      LED:   _/^^\________________________/^^...      raised-cosine bump, 25% wide
//             |<->| TREM_PULSE_WIDTH x cycle
//
//  The bump starts at the same instant the bend cuts in. It rises and falls along a
//  cosine curve, so there are no hard edges.
//
//  How the pieces fit:
//    - ledPwmBegin() hands D1 to the SAMD21's TCC0 timer at about 23 kHz (above hearing).
//    - setLedBrightness() is the ONLY function that writes the LED after that.
//    - serviceModeBlink() just keeps time for the blink. It doesn't touch the pin.
//    - serviceLed() picks a brightness each loop and calls setLedBrightness().
//
//  Note: if the device's own switch has the LED off, Q1 has nothing to darken or dim, so
//  none of this is visible. The bends and modes still work.
//
//  State (main tab): blinkRemaining, blinkPulsing, blinkStamp, tremStart, anyEngaged,
//                    anyStuttering
//  Writes: MAIN_LED (Q1 gate), through TCC0
//
// =====================================================================================

#include "wiring_private.h"   // pinPeripheral(): lets a pin be driven by a timer

// -------------------------------------------------------------------------------------
//  ledPwmBegin()  -  call once in setup(), after D1 has been set to a safe level
// -------------------------------------------------------------------------------------
//  Arduino's analogWrite() on this chip runs at about 732 Hz. That's in the audio band,
//  and Q1 switches the LED's current on the device's own power, so it could leak in as a
//  whine. Instead we set the timer up by hand:
//
//    clock    48 MHz (GCLK0, the main clock), no prescaler
//    mode     "normal PWM": the counter runs 0..LED_PWM_TOP, then wraps
//    output   D1 (PA04) is TCC0 output WO[0]. It goes HIGH (Q1 on = dark) when the
//             counter wraps, and LOW (Q1 off = lit) when the counter reaches CC[0]
//    result   48 MHz / (LED_PWM_TOP + 1) = about 23.4 kHz, and CC[0] sets how dark it is
//
//  Nothing else in this sketch uses TCC0 (the bend gates are plain digital outputs).
//  Every SYNCBUSY wait is there because the timer runs in its own clock domain, so a
//  register write isn't finished until it has synced across.
// -------------------------------------------------------------------------------------
void ledPwmBegin() {
  // 1. Feed the 48 MHz main clock to TCC0 (TCC0 and TCC1 share one clock channel).
  //    The timer's bus clock (APB) is already switched on by the Arduino core at boot.
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC0_TCC1;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // 2. Make sure the timer is stopped while we configure it.
  TCC0->CTRLA.bit.ENABLE = 0;
  while (TCC0->SYNCBUSY.bit.ENABLE);

  // 3. No prescaler: count at the full 48 MHz.
  TCC0->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1;

  // 4. Single-slope "normal PWM" waveform.
  TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
  while (TCC0->SYNCBUSY.bit.WAVE);

  // 5. Counter top. This sets the PWM frequency (see LED_PWM_TOP in config.h).
  TCC0->PER.reg = LED_PWM_TOP;
  while (TCC0->SYNCBUSY.bit.PER);

  // 6. Starting duty: the idle brightness, so the LED doesn't jump when the timer takes over.
  TCC0->CC[0].reg = brightnessToDuty(LED_IDLE_LEVEL);
  while (TCC0->SYNCBUSY.bit.CC0);

  // 7. Go. Then hand the pin from plain digital output to the timer. Doing it in this
  //    order means the pin switches straight onto a running, correctly set waveform.
  TCC0->CTRLA.bit.ENABLE = 1;
  while (TCC0->SYNCBUSY.bit.ENABLE);
  pinPeripheral(MAIN_LED, PIO_TIMER);   // PA04 -> peripheral function E = TCC0/WO[0]
}

// -------------------------------------------------------------------------------------
//  brightnessToDuty(b)
// -------------------------------------------------------------------------------------
//  Converts "how bright it should look" (0.0 dark .. 1.0 full) into the timer compare
//  value: the number of counts per PWM cycle that Q1 spends ON (dark).
//
//    1. clamp b to 0..1, so a slightly-off calculation can't overflow the register
//    2. gamma: eyes see brightness non-linearly. b^2.2 is the fraction of time the LED
//       should actually be lit for it to *look* like b.
//    3. Q1 ON means dark, so the dark time is (1 - lit fraction).
//
//  The result runs from 0 (Q1 never on = full) to LED_PWM_TOP + 1 (Q1 always on =
//  dark). A compare value past the top keeps the output HIGH for the whole cycle.
// -------------------------------------------------------------------------------------
uint32_t brightnessToDuty(float b) {
  if (b < 0.0f) b = 0.0f;
  if (b > 1.0f) b = 1.0f;
  float lit = powf(b, LED_GAMMA);
  return (uint32_t)((1.0f - lit) * (LED_PWM_TOP + 1) + 0.5f);   // +0.5 = round to nearest
}

// -------------------------------------------------------------------------------------
//  setLedBrightness(b)  -  the ONE place that drives the LED once the PWM is running
// -------------------------------------------------------------------------------------
//  Don't use digitalWrite(MAIN_LED, ...) after ledPwmBegin(). The pin now belongs to the
//  timer, and digitalWrite would silently do nothing.
//
//  The value goes into the *buffered* compare register (CCB). The timer copies it in at
//  the end of the current PWM cycle, so a change never cuts a cycle short (no glitch).
//  The last value is cached, because loop() calls this thousands of times a second and
//  most of the time the brightness hasn't changed.
// -------------------------------------------------------------------------------------
void setLedBrightness(float b) {
  static uint32_t lastDuty = 0xFFFFFFFF;   // impossible value, so the first call writes
  uint32_t duty = brightnessToDuty(b);
  if (duty == lastDuty) return;
  lastDuty = duty;

  while (TCC0->SYNCBUSY.bit.CCB0);   // wait until any previous buffered write has landed
  TCC0->CCB[0].reg = duty;
}

// -------------------------------------------------------------------------------------
//  startModeBlink(count)
// -------------------------------------------------------------------------------------
//  Starts (or restarts) a sequence of `count` dark dips. Returns right away.
//  serviceModeBlink() keeps the time, and serviceLed() does the actual dimming.
//  The first dip starts now.
// -------------------------------------------------------------------------------------
void startModeBlink(int count) {
  blinkRemaining = count;
  blinkPulsing = true;
  blinkStamp = millis();
}

// -------------------------------------------------------------------------------------
//  serviceModeBlink()
// -------------------------------------------------------------------------------------
//  Called once per loop. Steps the blink timing. It only flips blinkPulsing, and
//  serviceLed() turns that into light. Two states:
//
//    PULSING (dark):   after BLINK_PULSE_MS, one blink is finished. Count it down and
//                      start a gap.
//    GAP     (normal): if blinks remain, after BLINK_GAP_MS start the next dip.
//                      If none remain, the sequence is over.
//
//  The count goes down at the end of each dip, so the sequence always ends in a gap and
//  the LED goes straight back to its normal animation.
// -------------------------------------------------------------------------------------
void serviceModeBlink() {
  if (blinkPulsing) {
    if (elapsed(blinkStamp, BLINK_PULSE_MS)) {   // end of dip
      blinkPulsing = false;
      blinkRemaining--;
    }
  } else if (blinkRemaining > 0) {
    if (elapsed(blinkStamp, BLINK_GAP_MS)) {     // end of gap -> next dip
      blinkPulsing = true;
    }
  }
  // else: no blink running. Nothing to do.
}

// -------------------------------------------------------------------------------------
//  tremPulseLevel()
// -------------------------------------------------------------------------------------
//  The trem-mode LED brightness for right now: a raised-cosine bump at the start of
//  every stutter cycle, and idle for the rest of the cycle.
//
//    cycle  = 2 x the stutter interval   (one bend-on half + one bend-off half)
//    t      = where we are in the cycle   (same clock as the bends: tremStart)
//    width  = TREM_PULSE_WIDTH x cycle    (25% by default)
//
//  Inside the pulse, 0.5 x (1 - cos(2*pi * t / width)) goes smoothly 0 -> 1 -> 0, like
//  one breath. That shape is what rounds the edges. It's scaled to run from the idle
//  level up to the active level.
// -------------------------------------------------------------------------------------
float tremPulseLevel() {
  unsigned long cycle = 2 * tremInterval();
  unsigned long t = (millis() - tremStart) % cycle;
  float width = TREM_PULSE_WIDTH * cycle;

  if (t >= width) return LED_IDLE_LEVEL;   // between pulses

  float shape = 0.5f * (1.0f - cosf(2.0f * PI * t / width));   // 0 -> 1 -> 0
  return LED_IDLE_LEVEL + (LED_ACTIVE_LEVEL - LED_IDLE_LEVEL) * shape;
}

// -------------------------------------------------------------------------------------
//  serviceLed()  -  called once per loop, after the bends and the mode button
// -------------------------------------------------------------------------------------
//  Picks the LED brightness for this loop using the priority list at the top of this
//  tab, then writes it. Calling this every loop is what keeps the animation smooth.
//  setLedBrightness() skips the write when nothing changed.
// -------------------------------------------------------------------------------------
void serviceLed() {
  float level;

  if (blinkPulsing) {
    level = 0.0f;                                   // 1. mode blink dip: dark
  } else if (mode == MOMENTARY_TREM || mode == LATCHING_TREM) {
    level = anyStuttering ? tremPulseLevel()        // 2. stuttering: synced pulse
                          : LED_IDLE_LEVEL;         //    waiting
  } else {
    level = anyEngaged ? LED_ACTIVE_LEVEL           // 3. bend engaged: full
                       : LED_IDLE_LEVEL;            // 4. waiting: dim
  }

  setLedBrightness(level);
}

// -------------------------------------------------------------------------------------
//  t1BlinkTest()  -  bench test, only runs when T1_TEST is 1 (config.h)
// -------------------------------------------------------------------------------------
//  Swaps Q1 between fully off and fully on once a second (1 s lit, 1 s dark), so you can
//  check by eye that the LED hack is wired up. loop() calls only this while the test is
//  on. It goes through the PWM like everything else, so it also proves the timer setup
//  works. (The name T1_TEST is the older label for Q1. It's kept because the README
//  documents it.)
// -------------------------------------------------------------------------------------
void t1BlinkTest() {
  static unsigned long last = 0;   // `static` keeps the value between calls
  static bool lit = true;          // true = LED at full brightness

  if (elapsed(last, 1000)) {
    lit = !lit;
    setLedBrightness(lit ? 1.0f : 0.0f);
#if LOG
    Serial.println(lit ? "T1 lit (Q1 off)" : "T1 dark (Q1 on)");
#endif
  }
}
