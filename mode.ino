// =====================================================================================
//  mode.ino  -  the MODE button: step through the bend modes
// =====================================================================================
//
//  Each MODE press moves to the next mode (1 -> 2 -> 3 -> 4 -> 1 ...) and blinks the
//  main LED that many times so you know where you landed. The mode list is the `Mode`
//  enum in config.h. What each mode does is in bends.ino.
//
//  Reads:  the MODE button
//  Writes: `mode` (main tab), and starts the LED blink (led.ino)
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  serviceMode()
// -------------------------------------------------------------------------------------
//  Called once per loop. Does nothing unless MODE was just pressed.
//
//  A press during a blink restarts the blink with the new count, so a few quick taps
//  only ever show the final mode.
// -------------------------------------------------------------------------------------
void serviceMode() {
  if (!pressed(modeBtn)) return;

  // Wrap around: 1..3 step to the next mode, and 4 % 4 = 0, so 4 goes back to 1.
  mode = (mode % NUM_MODES) + 1;

  startModeBlink(mode);   // blink the main LED `mode` times

#if LOG
  Serial.print("MODE -> mode "); Serial.print(mode);
  Serial.print(" ("); Serial.print(MODE_NAMES[mode]); Serial.println(")");
#endif
}
