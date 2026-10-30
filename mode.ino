// =====================================================================================
//  mode.ino  -  the MODE button, plus small questions about the current mode
// =====================================================================================
//
//  Both act on release:
//    short press   next bend mode (ignored in speed mode)
//    long press    held at least LONG_PRESS_MS: enter or leave speed mode (speed.ino)
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  serviceMode()  -  called once per loop
// -------------------------------------------------------------------------------------
void serviceMode() {
  if (readButton(modeBtn) != BTN_RELEASE) return;

  if (pressLength(modeBtn) >= LONG_PRESS_MS) toggleSpeedMode();
  else if (!speedMode)                       nextMode();
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
