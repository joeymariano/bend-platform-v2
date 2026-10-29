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
//  Also sets anyEngaged when a bend is on.
// -------------------------------------------------------------------------------------
void setSwitch(Bend &b, bool on) {
  digitalWrite(b.gate, on ? BEND_ON : BEND_OFF);
  if (on) anyEngaged = true;
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
//  tremolo(b, interval)  -  stutter bend `b`: on `interval` ms, off `interval` ms
// -------------------------------------------------------------------------------------
//  All bends share one clock (tremStart):
//    - The first bend to stutter starts the clock, on the "on" half, so it cuts in at once.
//    - A bend that joins later uses the same clock, so both flip together.
//    - The LED pulse uses it too, so it lines up with the bends.
// -------------------------------------------------------------------------------------
void tremolo(Bend &b, unsigned long interval) {
  if (!tremRunning) {          // first to stutter: start the clock
    tremStart = millis();
    tremRunning = true;
  }
  anyStuttering = true;

  bool on = ((millis() - tremStart) / interval) % 2 == 0;   // even half = on
  setSwitch(b, on);
}

// -------------------------------------------------------------------------------------
//  serviceBends()  -  called once per loop
// -------------------------------------------------------------------------------------
//  Clears anyEngaged / anyStuttering, lets every bend set them again, then remembers
//  whether a stutter is running. Once every bend stops, the clock restarts on the
//  next stutter.
// -------------------------------------------------------------------------------------
void serviceBends() {
  anyEngaged = false;
  anyStuttering = false;

  for (int i = 0; i < NUM_BENDS; i++) serviceBend(bends[i], i);

  tremRunning = anyStuttering;
}

// -------------------------------------------------------------------------------------
//  serviceBend(b, idx)  -  read one bend's button and drive it for the current mode
// -------------------------------------------------------------------------------------
//  The gate is written on every pass, so a mode change takes effect at once.
//  `latched` is kept across mode changes. `idx` is only used for the log.
// -------------------------------------------------------------------------------------
void serviceBend(Bend &b, int idx) {
  bool justPressed = pressed(b.button);   // also updates b.button.down

  switch (mode) {
    case MOMENTARY:
      setSwitch(b, b.button.down);
      break;

    case MOMENTARY_TREM:
      if (b.button.down) tremolo(b, tremInterval());
      else               setSwitch(b, false);
      break;

    case LATCHING:
      if (justPressed) b.latched = !b.latched;
      setSwitch(b, b.latched);
      break;

    case LATCHING_TREM:
      if (justPressed) b.latched = !b.latched;
      if (b.latched) tremolo(b, tremInterval());
      else           setSwitch(b, false);
      break;
  }

#if LOG
  // e.g. "BTN-1 -> bend1 latched ON" or "BTN-2 -> bend2 momentary"
  if (justPressed) {
    Serial.print("BTN-"); Serial.print(idx + 1);
    Serial.print(" -> bend"); Serial.print(idx + 1);
    if (mode >= LATCHING) Serial.println(b.latched ? " latched ON" : " latched OFF");
    else { Serial.print(' '); Serial.println(MODE_NAMES[mode]); }
  }
#endif
}
