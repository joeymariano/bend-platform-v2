// =====================================================================================
//  bends.ino  -  drive each circuit bend according to the current mode
// =====================================================================================
//
//  Each bend is one CD4066 analog switch. A XIAO output drives a MOSFET gate (Q2/Q3),
//  and that MOSFET drives the CD4066 control pin. Code here only ever says "bend on" or
//  "bend off". setSwitch() turns that into the right HIGH/LOW for the board revision
//  (BEND_ON / BEND_OFF in config.h).
//
//  Reads:  the current `mode` (main tab), each bend's button
//  Writes: the bend gate pins (Q3 = bend 1, Q2 = bend 2), plus the shared stutter clock
//          and the anyEngaged / anyStuttering flags that led.ino animates from
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  setSwitch(b, on)
// -------------------------------------------------------------------------------------
//  Engages (on = true) or releases (on = false) bend `b`. This is the only place that
//  writes to a bend gate, so the board polarity is handled in one spot.
//
//  It also notes when any bend is engaged (anyEngaged). led.ino uses that to light the
//  LED fully in the momentary and latching modes.
// -------------------------------------------------------------------------------------
void setSwitch(Bend &b, bool on) {
  digitalWrite(b.gate, on ? BEND_ON : BEND_OFF);
  if (on) anyEngaged = true;
}

// -------------------------------------------------------------------------------------
//  tremInterval()
// -------------------------------------------------------------------------------------
//  The stutter half-period (ms) for the current mode. The bends and the LED animation
//  both ask this one function, so they can never disagree about the speed.
// -------------------------------------------------------------------------------------
unsigned long tremInterval() {
  return (mode == MOMENTARY_TREM) ? TREM_MS : LATCH_TREM_MS;
}

// -------------------------------------------------------------------------------------
//  tremolo(b, interval)
// -------------------------------------------------------------------------------------
//  Makes the bend stutter: on for `interval` ms, off for `interval` ms, repeating (a
//  square wave). Call it every loop while the stutter should run. It never blocks.
//
//  All bends share ONE stutter clock (tremStart, in the main tab) instead of each keeping
//  its own timer:
//    - When the first bend starts stuttering, the clock restarts at "now". The first
//      half-cycle is "on", so a press cuts in immediately.
//    - A second bend that joins later locks onto the same clock, so both flip together.
//    - The LED animation (led.ino) reads the same clock, so its pulse lines up with
//      the bend cutting in.
//
//  The phase is plain arithmetic on millis(): which half-period are we in, and is that
//  number even (on) or odd (off)?
// -------------------------------------------------------------------------------------
void tremolo(Bend &b, unsigned long interval) {
  if (!tremRunning) {          // nobody was stuttering: start the shared clock now
    tremStart = millis();
    tremRunning = true;        // later bends in this same pass join this clock
  }
  anyStuttering = true;        // tells loop() and led.ino that a stutter is live

  bool on = ((millis() - tremStart) / interval) % 2 == 0;
  setSwitch(b, on);
}

// -------------------------------------------------------------------------------------
//  serviceBends()
// -------------------------------------------------------------------------------------
//  Called once per loop. Services every bend, then works out the summary flags led.ino
//  reads:
//    anyEngaged     at least one bend gate is on right now
//    anyStuttering  at least one bend is in a stutter
//
//  Both flags are cleared first, and setSwitch() / tremolo() set them again during the
//  pass, so they always describe *this* loop. Afterwards tremRunning remembers whether a
//  stutter was live. When every bend stops, the shared clock is free to restart cleanly
//  on the next press.
// -------------------------------------------------------------------------------------
void serviceBends() {
  anyEngaged = false;
  anyStuttering = false;

  for (int i = 0; i < NUM_BENDS; i++) serviceBend(bends[i], i);

  tremRunning = anyStuttering;
}

// -------------------------------------------------------------------------------------
//  serviceBend(b, idx)
// -------------------------------------------------------------------------------------
//  Called by serviceBends() for each bend. It reads that bend's button and sets the bend's
//  state for the current mode. `idx` (0 or 1) is only used for the serial log.
//
//  Every branch writes the gate on every pass, even when nothing changed. That keeps the
//  logic simple, and it means a mode change takes effect immediately. For example, going
//  from latching-trem back to momentary releases a stuck-on bend on the next pass.
//
//  The latched flag stays as it is across mode changes, so a bend latched in mode 3 is
//  still latched when you cycle around to mode 4.
// -------------------------------------------------------------------------------------
void serviceBend(Bend &b, int idx) {
  bool justPressed = pressed(b.button);   // also refreshes b.button.down

  switch (mode) {
    case MOMENTARY:
      // The bend follows the button: held = on, released = off.
      setSwitch(b, b.button.down);
      break;

    case MOMENTARY_TREM:
      // Stutters while held, steady off when released.
      if (b.button.down) tremolo(b, tremInterval());
      else               setSwitch(b, false);
      break;

    case LATCHING:
      // Each press flips the latch. The bend holds that state until the next press.
      if (justPressed) b.latched = !b.latched;
      setSwitch(b, b.latched);
      break;

    case LATCHING_TREM:
      // Each press flips the latch. While latched, the bend stutters.
      if (justPressed) b.latched = !b.latched;
      if (b.latched) tremolo(b, tremInterval());
      else           setSwitch(b, false);
      break;
  }

#if LOG
  // One line per press, e.g. "BTN-1 -> bend1 latched ON" or "BTN-2 -> bend2 momentary".
  if (justPressed) {
    Serial.print("BTN-"); Serial.print(idx + 1);
    Serial.print(" -> bend"); Serial.print(idx + 1);
    if (mode >= LATCHING) Serial.println(b.latched ? " latched ON" : " latched OFF");
    else { Serial.print(' '); Serial.println(MODE_NAMES[mode]); }
  }
#endif
}
