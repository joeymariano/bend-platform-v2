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
//  Writes: the bend gate pins (Q3 = bend 1, Q2 = bend 2)
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  setSwitch(b, on)
// -------------------------------------------------------------------------------------
//  Engages (on = true) or releases (on = false) bend `b`. This is the only place that
//  writes to a bend gate, so the board polarity is handled in one spot.
// -------------------------------------------------------------------------------------
void setSwitch(Bend &b, bool on) {
  digitalWrite(b.gate, on ? BEND_ON : BEND_OFF);
}

// -------------------------------------------------------------------------------------
//  tremolo(b, interval)
// -------------------------------------------------------------------------------------
//  Makes the bend stutter: it flips between on and off every `interval` ms, which is a
//  square wave. Call it every loop while the stutter should run. It uses elapsed() from
//  buttons.ino, so it never blocks.
//
//  The phase is remembered per bend, so stopping and restarting continues from where it
//  left off instead of always starting "on". That isn't audible at these speeds.
// -------------------------------------------------------------------------------------
void tremolo(Bend &b, unsigned long interval) {
  if (elapsed(b.lastTrem, interval)) b.tremPhase = !b.tremPhase;
  setSwitch(b, b.tremPhase);
}

// -------------------------------------------------------------------------------------
//  serviceBend(b, idx)
// -------------------------------------------------------------------------------------
//  Called once per loop for each bend. It reads that bend's button and sets the bend's
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
      if (b.button.down) tremolo(b, TREM_MS);
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
      if (b.latched) tremolo(b, LATCH_TREM_MS);
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
