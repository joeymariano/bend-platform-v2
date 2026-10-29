// =====================================================================================
//  mode.ino  -  the MODE button
// =====================================================================================

// -------------------------------------------------------------------------------------
//  serviceMode()  -  called once per loop
// -------------------------------------------------------------------------------------
//  On a MODE press: step to the next mode and blink it. A press mid-blink restarts the
//  blink, so quick taps only show the final mode.
// -------------------------------------------------------------------------------------
void serviceMode() {
  if (!pressed(modeBtn)) return;

  mode = (mode % NUM_MODES) + 1;   // 1 -> 2 -> 3 -> 4 -> 1

  startModeBlink(mode);

#if LOG
  Serial.print("MODE -> mode "); Serial.print(mode);
  Serial.print(" ("); Serial.print(MODE_NAMES[mode]); Serial.println(")");
#endif
}
