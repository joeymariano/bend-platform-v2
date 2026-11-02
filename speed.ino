// =====================================================================================
//  speed.ino  -  speed mode: set how fast the stutter (and its LED pulse) runs
// =====================================================================================
//
//  Long-press MODE to enter (from mode 2 or 4 only) or leave (mode.ino). The LED
//  flashes fast either way. In speed mode:
//    - the bends keep running, at the new speed; presses don't toggle a latch (bends.ino)
//    - the LED pulses at the current speed, as a guide (led.ino)
//    - BTN-1 slows it down, BTN-2 speeds it up (each acts on release)
//
//  The speed multiplies both trem modes' rates, so they change together. It's saved to
//  flash on leaving speed mode, and loaded at power-up (flash.ino).
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  setSpeedSteps(n)  -  the only place that sets the speed
// -------------------------------------------------------------------------------------
void setSpeedSteps(int n) {
  speedSteps = constrain(n, -SPEED_MAX_STEPS, SPEED_MAX_STEPS);
  tremSpeed = powf(SPEED_STEP, speedSteps);
}

// -------------------------------------------------------------------------------------
//  toggleSpeedMode()  -  enter or leave speed mode
// -------------------------------------------------------------------------------------
//  Leaving saves the speed first, so the flash write doesn't eat into the fast flash.
// -------------------------------------------------------------------------------------
void toggleSpeedMode() {
  speedMode = !speedMode;
  if (!speedMode) saveIfChanged();  // flash.ino
  tremStart = millis();   // the pulse starts fresh
  startFastFlash();       // led.ino

#if LOG
  Serial.print(speedMode ? "SPEED mode ON, " : "SPEED mode OFF, ");
  Serial.print(tremSpeed, 2); Serial.println("x");
#endif
}

// -------------------------------------------------------------------------------------
//  speedButton(idx, edge)  -  a bend button in speed mode (called by serviceBends)
// -------------------------------------------------------------------------------------
//  On release: BTN-1 (idx 0) one step slower, BTN-2 (idx 1) one step faster.
// -------------------------------------------------------------------------------------
void speedButton(int idx, ButtonEdge edge) {
  if (edge != BTN_RELEASE) return;

  int dir = (idx == 0) ? -1 : +1;
  setSpeedSteps(speedSteps + dir);
  tremStart = millis();   // restart the pulse, so the new speed shows at once

#if LOG
  Serial.print("BTN-"); Serial.print(idx + 1);
  Serial.print(dir < 0 ? " -> slower, " : " -> faster, ");
  Serial.print(tremSpeed, 2); Serial.println("x");
#endif
}
