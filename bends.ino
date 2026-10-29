// =====================================================================================
//  bends.ino  -  drive each bend according to the current mode
// =====================================================================================
//
//  Reads:  `mode`, each bend's button
//  Writes: the bend gates, the stutter clock, anyEngaged / anyStuttering
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  setSwitch(b, on)  -  the only place that writes a bend gate
// -------------------------------------------------------------------------------------
//  Turns bend `b` on or off, using BEND_ON / BEND_OFF for the board's polarity.
// -------------------------------------------------------------------------------------
void setSwitch(Bend &b, bool on) {
  digitalWrite(b.gate, on ? BEND_ON : BEND_OFF);
}

// -------------------------------------------------------------------------------------
//  tremInterval()  -  stutter half-period (ms) for the current mode
// -------------------------------------------------------------------------------------
//  The bends and the LED both use this, so they always agree on the speed.
// -------------------------------------------------------------------------------------
unsigned long tremInterval() {
  return (mode == MOMENTARY_TREM) ? TREM_MS : LATCH_TREM_MS;
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
//  Drives every bend, then sums up what they're doing for led.ino. Once no bend is
//  stuttering, the clock restarts on the next stutter.
// -------------------------------------------------------------------------------------
void serviceBends() {
  anyEngaged = false;
  anyStuttering = false;

  for (int i = 0; i < NUM_BENDS; i++) {
    serviceBend(bends[i], i);
    if (bends[i].on)         anyEngaged = true;
    if (bends[i].stuttering) anyStuttering = true;
  }

  tremRunning = anyStuttering;
}

// -------------------------------------------------------------------------------------
//  serviceBend(b, idx)  -  read one bend's button and drive it for the current mode
// -------------------------------------------------------------------------------------
//  Two questions decide everything:
//    wanted?   momentary modes: is the button held?  latching modes: is it toggled on?
//    stutter?  in the trem modes, a wanted bend follows the stutter clock
//
//  The gate is written on every pass, so a mode change takes effect at once.
//  `latched` is kept across mode changes. `idx` is only used for the log.
// -------------------------------------------------------------------------------------
void serviceBend(Bend &b, int idx) {
  bool justPressed = pressed(b.button);   // also updates b.button.down

  if (isLatchingMode() && justPressed) b.latched = !b.latched;

  bool wanted = isLatchingMode() ? b.latched : b.button.down;
  b.stuttering = wanted && isTremMode();
  b.on = b.stuttering ? tremPhaseOn() : wanted;
  setSwitch(b, b.on);

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
