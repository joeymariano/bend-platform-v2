// =====================================================================================
//  led.ino  -  mode indicator on the device's main LED (via Q1)
// =====================================================================================
//
//  There's no separate mode LED. Instead Q1 can briefly short out the device's own
//  bypass LED. The LED normally rests lit (LED_IDLE). A blink is a short dark pulse
//  (LED_PULSE). Mode N = N dark pulses.
//
//  Timeline for mode 3 (# = dark pulse, . = lit gap):
//
//      ###...###...###      then back to rest (lit)
//      |-| BLINK_PULSE_MS
//         |-| BLINK_GAP_MS
//
//  The blink runs as a small state machine stepped by serviceModeBlink() every loop. It
//  never uses delay(), so the bends keep responding while the LED blinks.
//
//  Note: if the device's own switch has the LED off, there's nothing to darken and the
//  blinks won't be visible. The mode still changes.
//
//  State (main tab): blinkRemaining, blinkPulsing, blinkStamp
//  Writes: MAIN_LED (Q1 gate)
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  startModeBlink(count)
// -------------------------------------------------------------------------------------
//  Starts (or restarts) a sequence of `count` dark pulses. Returns right away, and
//  serviceModeBlink() does the rest. The first pulse starts now.
// -------------------------------------------------------------------------------------
void startModeBlink(int count) {
  blinkRemaining = count;
  blinkPulsing = true;
  blinkStamp = millis();
  digitalWrite(MAIN_LED, LED_PULSE);    // first pulse starts immediately
}

// -------------------------------------------------------------------------------------
//  serviceModeBlink()
// -------------------------------------------------------------------------------------
//  Called once per loop. Two states:
//
//    PULSING (LED dark):  after BLINK_PULSE_MS, light the LED again. That finishes one
//                         blink, so count it down and start a gap.
//    GAP     (LED lit):   if blinks remain, after BLINK_GAP_MS start the next pulse.
//                         If none remain, the sequence is done and the LED stays at rest.
//
//  The count goes down at the end of each pulse, so the LED is always lit (at rest) when
//  the sequence finishes.
// -------------------------------------------------------------------------------------
void serviceModeBlink() {
  if (blinkPulsing) {
    if (elapsed(blinkStamp, BLINK_PULSE_MS)) {
      digitalWrite(MAIN_LED, LED_IDLE);     // end of pulse -> LED lit again
      blinkPulsing = false;
      blinkRemaining--;
    }
  } else if (blinkRemaining > 0) {
    if (elapsed(blinkStamp, BLINK_GAP_MS)) {
      digitalWrite(MAIN_LED, LED_PULSE);    // end of gap -> next pulse
      blinkPulsing = true;
    }
  }
  // else: no blink running. Nothing to do.
}

// -------------------------------------------------------------------------------------
//  t1BlinkTest()  -  bench test, only runs when T1_TEST is 1 (config.h)
// -------------------------------------------------------------------------------------
//  Toggles Q1 once a second (1 s lit, 1 s dark), so you can check by eye that the
//  LED-interrupt wiring works. loop() calls only this while the test is on. (The name
//  T1_TEST is the older label for Q1. It's kept because the README documents it.)
// -------------------------------------------------------------------------------------
void t1BlinkTest() {
  static unsigned long last = 0;   // `static` keeps the value between calls
  static bool idle = true;         // true = LED at rest (lit)

  if (elapsed(last, 1000)) {
    idle = !idle;
    digitalWrite(MAIN_LED, idle ? LED_IDLE : LED_PULSE);
#if LOG
    Serial.println(idle ? "T1 idle (rest)" : "T1 pulse");
#endif
  }
}
