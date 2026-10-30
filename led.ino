// =====================================================================================
//  led.ino  -  the device's LED (via Q1): dimming, blinks, bend animation
// =====================================================================================
//
//  Q1 can pull the device's LED to ground. Switching it fast (PWM) sets any brightness.
//
//  What the LED shows, highest priority first:
//    1. blinks        N dark blinks for mode N, or the fast flash for speed mode
//    2. speed mode    the trem pulse, nonstop, at the current speed
//    3. trem modes    a soft pulse at the start of each stutter cycle
//    4. other modes   full while either bend is on
//    5. waiting       dim (LED_IDLE_LEVEL)
//
//  Mode blink for mode 3 (# = dark, . = animation):
//
//      ###...###...###      then back to the animation
//      |-| BLINK_PULSE_MS
//         |-| BLINK_GAP_MS
//
//  The fast flash is the same, with FLASH_COUNT blinks of FLASH_MS each.
//
//  Trem pulse over one stutter cycle:
//
//      bend:  |^^^^^^^^^^^^^|_____________|^^^^...
//      LED:   _/^^\________________________/^^...
//             |<->| TREM_PULSE_WIDTH x cycle
//
//  If the device's own switch has the LED off, none of this shows. The bends still work.
//
// =====================================================================================

#include "wiring_private.h"   // pinPeripheral()

// Blink state.
int blinkRemaining = 0;          // blinks left, including the one in progress
bool blinkPulsing = false;       // true while the LED is dark for a blink
unsigned long blinkStamp = 0;    // millis() when the current blink or gap began
unsigned long blinkDarkMs = 0;   // dark time per blink
unsigned long blinkLitMs = 0;    // lit time between blinks

// -------------------------------------------------------------------------------------
//  ledPwmBegin()  -  call once in setup()
// -------------------------------------------------------------------------------------
//  analogWrite() runs at about 732 Hz, which could leak into the audio as a whine. So
//  TCC0 is set up by hand at 48 MHz / (LED_PWM_TOP + 1) = 23.4 kHz. Output D1 (PA04 =
//  TCC0/WO[0]) is HIGH (Q1 on = dark) from the start of each cycle until the count
//  reaches CC[0].
//
//  The SYNCBUSY waits let each register write finish (the timer has its own clock).
// -------------------------------------------------------------------------------------
void ledPwmBegin() {
  // Feed the 48 MHz main clock to TCC0.
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC0_TCC1;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Stop the timer while configuring it.
  TCC0->CTRLA.bit.ENABLE = 0;
  while (TCC0->SYNCBUSY.bit.ENABLE);

  // No prescaler.
  TCC0->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1;

  // Normal (single-slope) PWM.
  TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
  while (TCC0->SYNCBUSY.bit.WAVE);

  // Counter top: sets the frequency.
  TCC0->PER.reg = LED_PWM_TOP;
  while (TCC0->SYNCBUSY.bit.PER);

  // Start at idle brightness, so the LED doesn't jump.
  TCC0->CC[0].reg = brightnessToDuty(LED_IDLE_LEVEL);
  while (TCC0->SYNCBUSY.bit.CC0);

  // Start the timer, then hand it the pin, so the pin joins a running waveform.
  TCC0->CTRLA.bit.ENABLE = 1;
  while (TCC0->SYNCBUSY.bit.ENABLE);
  pinPeripheral(LED_GATE, PIO_TIMER);
}

// -------------------------------------------------------------------------------------
//  brightnessToDuty(b)  -  brightness (0.0..1.0) to the timer's compare value
// -------------------------------------------------------------------------------------
//  Clamps b, applies gamma (so 0.5 looks half as bright), then inverts, because Q1 on
//  means dark. Returns 0 (full) up to LED_PWM_TOP + 1 (dark).
// -------------------------------------------------------------------------------------
uint32_t brightnessToDuty(float b) {
  if (b < 0.0f) b = 0.0f;
  if (b > 1.0f) b = 1.0f;
  float lit = powf(b, LED_GAMMA);
  return (uint32_t)((1.0f - lit) * (LED_PWM_TOP + 1) + 0.5f);   // round to nearest
}

// -------------------------------------------------------------------------------------
//  setLedBrightness(b)  -  the only place that writes the LED after ledPwmBegin()
// -------------------------------------------------------------------------------------
//  (digitalWrite() no longer works on D1 once the timer owns it.)
//  Writes the buffered register (CCB), which takes effect at the end of the current
//  cycle, so there's no glitch. Skips the write if nothing changed.
// -------------------------------------------------------------------------------------
void setLedBrightness(float b) {
  static uint32_t lastDuty = 0xFFFFFFFF;   // impossible value, so the first call writes
  uint32_t duty = brightnessToDuty(b);
  if (duty == lastDuty) return;
  lastDuty = duty;

  while (TCC0->SYNCBUSY.bit.CCB0);   // wait for the previous write
  TCC0->CCB[0].reg = duty;
}

// -------------------------------------------------------------------------------------
//  startBlink(count, darkMs, litMs)  -  start (or restart) `count` dark blinks, now
// -------------------------------------------------------------------------------------
void startBlink(int count, unsigned long darkMs, unsigned long litMs) {
  blinkRemaining = count;
  blinkDarkMs = darkMs;
  blinkLitMs = litMs;
  blinkPulsing = true;
  blinkStamp = millis();
}

// N blinks for mode N.
void startModeBlink(int count) {
  startBlink(count, BLINK_PULSE_MS, BLINK_GAP_MS);
}

// Fast flash: entering or leaving speed mode.
void startFastFlash() {
  startBlink(FLASH_COUNT, FLASH_MS, FLASH_MS);
}

// -------------------------------------------------------------------------------------
//  serviceBlink()  -  called once per loop; only keeps time
// -------------------------------------------------------------------------------------
//  Dark: after blinkDarkMs, count one blink done and start a gap.
//  Gap:  if blinks remain, after blinkLitMs start the next one.
//  After the last blink the LED goes straight back to the animation.
// -------------------------------------------------------------------------------------
void serviceBlink() {
  if (blinkPulsing) {
    if (elapsed(blinkStamp, blinkDarkMs)) {   // blink over
      blinkPulsing = false;
      blinkRemaining--;
    }
  } else if (blinkRemaining > 0) {
    if (elapsed(blinkStamp, blinkLitMs)) {    // gap over
      blinkPulsing = true;
    }
  }
}

// -------------------------------------------------------------------------------------
//  tremPulseLevel()  -  LED brightness for the trem modes and speed mode, right now
// -------------------------------------------------------------------------------------
//  A smooth bump (raised cosine) from idle up to active and back, at the start of each
//  stutter cycle. Idle for the rest of the cycle.
// -------------------------------------------------------------------------------------
float tremPulseLevel() {
  unsigned long cycle = 2 * tremInterval();                // on half + off half
  unsigned long t = (millis() - tremStart) % cycle;        // position in the cycle
  float width = TREM_PULSE_WIDTH * cycle;

  if (t >= width) return LED_IDLE_LEVEL;   // between pulses

  float shape = 0.5f * (1.0f - cosf(2.0f * PI * t / width));   // 0 -> 1 -> 0
  return LED_IDLE_LEVEL + (LED_ACTIVE_LEVEL - LED_IDLE_LEVEL) * shape;
}

// -------------------------------------------------------------------------------------
//  serviceLed()  -  called once per loop, after the bends
// -------------------------------------------------------------------------------------
//  Picks this loop's brightness from the priority list at the top of this tab.
// -------------------------------------------------------------------------------------
void serviceLed() {
  float level;

  if (blinkPulsing) {
    level = 0.0f;                                   // 1. blink: dark
  } else if (speedMode) {
    level = tremPulseLevel();                       // 2. speed mode: pulse
  } else if (isTremMode()) {
    level = anyStuttering ? tremPulseLevel()        // 3. stuttering: pulse
                          : LED_IDLE_LEVEL;         //    waiting
  } else {
    level = anyEngaged ? LED_ACTIVE_LEVEL           // 4. bend on: full
                       : LED_IDLE_LEVEL;            // 5. waiting: dim
  }

  setLedBrightness(level);
}
