// =====================================================================================
//  mode.ino  -  the MODE button, plus small questions about the current mode
// =====================================================================================
//
//  short press   acts on release: next bend mode (ignored in speed mode)
//  long press    acts the moment it reaches LONG_PRESS_MS, still held: enter or leave
//                speed mode (speed.ino). The release that follows does nothing.
//
// =====================================================================================

bool modeLongDone = false;   // this press already counted as a long press

// -------------------------------------------------------------------------------------
//  serviceMode()  -  called once per loop
// -------------------------------------------------------------------------------------
void serviceMode() {
  ButtonEdge edge = readButton(modeBtn);

  if (modeBtn.down && !modeLongDone && pressLength(modeBtn) >= LONG_PRESS_MS) {
    modeLongDone = true;
    toggleSpeedMode();
  }

  if (edge == BTN_RELEASE) {
    if (!modeLongDone && !speedMode) nextMode();   // it was a short press
    modeLongDone = false;                          // ready for the next press
  }
}

// -------------------------------------------------------------------------------------
//  nextMode()  -  step to the next mode and blink it
// -------------------------------------------------------------------------------------
//  A press mid-blink restarts the blink, so quick taps only show the final mode.
// -------------------------------------------------------------------------------------
void nextMode() {
  mode = (mode % NUM_MODES) + 1;   // 1 -> 2 -> 3 -> 4 -> 1
  startModeBlink(mode);
  logMode("MODE -> mode ");
}

// Does a press toggle the bend (instead of holding it)?
bool isLatchingMode() {
  return mode == LATCHING || mode == LATCHING_TREM;
}

// Does an active bend stutter (instead of staying on)?
bool isTremMode() {
  return mode == MOMENTARY_TREM || mode == LATCHING_TREM;
}

// Logs e.g. "MODE -> mode 3 (latching)". Does nothing when LOG is 0.
void logMode(const char *prefix) {
#if LOG
  Serial.print(prefix); Serial.print(mode);
  Serial.print(" ("); Serial.print(MODE_NAMES[mode]); Serial.println(")");
#endif
}
