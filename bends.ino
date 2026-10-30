// =====================================================================================
//  bends.ino  -  drive each bend according to the current mode
// =====================================================================================
//
//  Reads:  `mode`, `speedMode`, each bend's button
//  Writes: the bend gates, the stutter clock, anyEngaged / anyStuttering
//
//  In speed mode the bends stay off and their buttons set the speed (speed.ino).
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  setBend(b, on, stuttering)  -  the only place that sets what a bend is doing
// -------------------------------------------------------------------------------------
//  Records it for this loop (led.ino reads it) and writes the gate, using
//  BEND_ON / BEND_OFF for the board's polarity.
// -------------------------------------------------------------------------------------
void setBend(Bend &b, bool on, bool stuttering) {
  b.on = on;
  b.stuttering = stuttering;
  digitalWrite(b.gate, on ? BEND_ON : BEND_OFF);
}

// -------------------------------------------------------------------------------------
//  tremInterval()  -  stutter half-period (ms) for the current mode
// -------------------------------------------------------------------------------------
//  The bends and the LED both use this, so they always agree on the speed.
//  tremSpeed (speed mode) scales it: 2.0 = twice as fast = half the interval.
// -------------------------------------------------------------------------------------
unsigned long tremInterval() {
  unsigned long normal = (mode == MOMENTARY_TREM) ? TREM_MS : LATCH_TREM_MS;
  return (unsigned long)(normal / tremSpeed + 0.5f);   // round to nearest ms
}

// -------------------------------------------------------------------------------------
//  tremPhaseOn()  -  is the shared stutter clock in its "on" half right now?
// -------------------------------------------------------------------------------------
//  All bends share one clock (tremStart):
//    - The first bend to stutter starts the clock, on the "on" half, so it cuts in at once.
//    - A bend that joins later uses the same clock, so both flip together.
//    - The LED pulse uses it too, so it lines up with the bends.
// -------------------------------------------------------------------------------------
bool tremPhaseOn() {
  if (!tremRunning) {          // first to stutter: start the clock
    tremStart = millis();
    tremRunning = true;
  }
  return ((millis() - tremStart) / tremInterval()) % 2 == 0;   // even half = on
}

// -------------------------------------------------------------------------------------
//  serviceBends()  -  called once per loop
// -------------------------------------------------------------------------------------
//  Reads each bend's button and drives the bend, then sums up what they're doing for
//  led.ino. Once no bend is stuttering, the clock restarts on the next stutter.
// -------------------------------------------------------------------------------------
void serviceBends() {
  anyEngaged = false;
  anyStuttering = false;

  for (int i = 0; i < NUM_BENDS; i++) {
    Bend &b = bends[i];
    ButtonEdge edge = readButton(b.button);   // also updates b.button.down

    if (speedMode) {
      speedButton(i, edge);        // the button sets the speed instead (speed.ino)
      setBend(b, false, false);    // bends stay off
    } else {
      serviceBend(b, edge, i);
    }

    if (b.on)         anyEngaged = true;
    if (b.stuttering) anyStuttering = true;
  }

  tremRunning = anyStuttering;
}

// -------------------------------------------------------------------------------------
//  serviceBend(b, edge, idx)  -  drive one bend for the current mode
// -------------------------------------------------------------------------------------
//  Two questions decide everything:
//    wanted?   momentary modes: is the button held?  latching modes: is it toggled on?
//    stutter?  in the trem modes, a wanted bend follows the stutter clock
//
//  Acts on contact (BTN_PRESS), not release, so the bend cuts in at once.
//  The gate is written on every pass, so a mode change takes effect at once.
//  `latched` is kept across mode changes. `idx` is only used for the log.
// -------------------------------------------------------------------------------------
void serviceBend(Bend &b, ButtonEdge edge, int idx) {
  bool justPressed = (edge == BTN_PRESS);

  if (isLatchingMode() && justPressed) b.latched = !b.latched;

  bool wanted = isLatchingMode() ? b.latched : b.button.down;
  bool stuttering = wanted && isTremMode();
  setBend(b, stuttering ? tremPhaseOn() : wanted, stuttering);

#if LOG
  // e.g. "BTN-1 -> bend1 latched ON" or "BTN-2 -> bend2 momentary"
  if (justPressed) {
    Serial.print("BTN-"); Serial.print(idx + 1);
    Serial.print(" -> bend"); Serial.print(idx + 1);
    if (isLatchingMode()) Serial.println(b.latched ? " latched ON" : " latched OFF");
    else { Serial.print(' '); Serial.println(MODE_NAMES[mode]); }
  }
#endif
}
