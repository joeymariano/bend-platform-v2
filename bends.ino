// =====================================================================================
//  bends.ino  -  drive each bend according to the current mode (mode 5: sequence.ino)
// =====================================================================================
//
//  Reads:  `mode`, `speedMode`, each bend's button (already read by readButtons())
//  Writes: the bend gates, the stutter clock, anyEngaged / anyStuttering
//
//  In speed mode the bends keep running, so you hear the new speed at once. Their
//  buttons set the speed (speed.ino) and don't toggle a latch.
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
//  tremInterval()  -  stutter half-period (or sequencer step) in ms, for the current mode
// -------------------------------------------------------------------------------------
//  The bends and the LED both use this, so they always agree on the speed.
//  tremSpeed (speed mode) scales it: 2.0 = twice as fast = half the interval.
// -------------------------------------------------------------------------------------
unsigned long tremInterval() {
  unsigned long normal = (mode == MOMENTARY_TREM) ? TREM_MS
                       : (mode == SEQUENCER)      ? SEQ_STEP_MS
                       :                            LATCH_TREM_MS;
  return (unsigned long)(normal / tremSpeed + 0.5f);   // round to nearest ms
}

// -------------------------------------------------------------------------------------
//  clockTicks()  -  how many tremInterval()s the shared stutter clock has run
// -------------------------------------------------------------------------------------
//  All bends share one clock (tremStart):
//    - The first bend to follow it starts it at tick 0.
//    - A bend that joins later uses the same clock, so both flip together.
//    - The LED pulse uses it too, so it lines up with the bends.
// -------------------------------------------------------------------------------------
unsigned long clockTicks() {
  if (!tremRunning) {          // first to follow the clock: start it
    tremStart = millis();
    tremRunning = true;
  }
  return (millis() - tremStart) / tremInterval();
}

// Is the stutter in its "on" half? Tick 0 is on, so a stutter cuts in at once.
bool tremPhaseOn() {
  return clockTicks() % 2 == 0;
}

// -------------------------------------------------------------------------------------
//  serviceBends()  -  called once per loop
// -------------------------------------------------------------------------------------
//  Hands the button edges to speed mode or the current bend mode, then sums up what the
//  bends are doing for led.ino. Once no bend follows the clock, it restarts next time.
// -------------------------------------------------------------------------------------
void serviceBends() {
  // This loop's button edges (read by readButtons()).
  ButtonEdge edges[NUM_BENDS];
  for (int i = 0; i < NUM_BENDS; i++) {
    edges[i] = bends[i].button.edge;
    if (speedMode) {
      speedButton(i, edges[i]);   // the press sets the speed (speed.ino)...
      edges[i] = BTN_NONE;        // ...so the bend doesn't see it: no latch toggle
    }
  }

  // Drive the bends: mode 5 plays its pattern, modes 1-4 drive each bend on its own.
  if (mode == SEQUENCER) serviceSequence(edges);   // sequence.ino
  else for (int i = 0; i < NUM_BENDS; i++) serviceBend(bends[i], edges[i], i);

  // Sum up for led.ino.
  anyEngaged = false;
  anyStuttering = false;
  for (int i = 0; i < NUM_BENDS; i++) {
    if (bends[i].on)         anyEngaged = true;
    if (bends[i].stuttering) anyStuttering = true;
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
