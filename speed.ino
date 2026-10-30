// =====================================================================================
//  speed.ino  -  speed mode: set how fast the stutter (and its LED pulse) runs
// =====================================================================================
//
//  Long-press MODE to enter or leave (mode.ino). The LED flashes fast either way.
//  In speed mode:
//    - the bends stay off (bends.ino)
//    - the LED pulses at the current speed, as a guide (led.ino)
//    - BTN-1 slows it down, BTN-2 speeds it up (each acts on release)
//
//  The speed multiplies both trem modes' rates, so they change together. It's saved to
//  flash on leaving speed mode, and loaded at power-up (flash.ino).
//
// =====================================================================================

int speedSteps = 0;   // steps from normal speed: negative = slower, positive = faster
int savedSteps = 0;   // what's in flash, so it's saved only when it changed

// -------------------------------------------------------------------------------------
//  setSpeedSteps(n)  -  the only place that sets the speed
// -------------------------------------------------------------------------------------
void setSpeedSteps(int n) {
  speedSteps = constrain(n, -SPEED_MAX_STEPS, SPEED_MAX_STEPS);
  tremSpeed = powf(SPEED_STEP, speedSteps);
}

// -------------------------------------------------------------------------------------
//  loadSpeed()  -  call once in setup(): the last saved speed, or 1.0x
// -------------------------------------------------------------------------------------
void loadSpeed() {
  Settings s;
  bool found = loadSettings(s);
  if (found) setSpeedSteps(s.speedSteps);   // out of range? setSpeedSteps clamps it
  savedSteps = speedSteps;

#if LOG
  Serial.print("speed "); Serial.print(tremSpeed, 2);
  Serial.println(found ? "x (saved)" : "x (default)");
#endif
}

// -------------------------------------------------------------------------------------
//  saveSpeed()  -  save the speed to flash, if it changed
// -------------------------------------------------------------------------------------
void saveSpeed() {
  if (speedSteps == savedSteps) return;   // unchanged: spare the flash

  Settings s = { speedSteps };
  saveSettings(s);
  savedSteps = speedSteps;

#if LOG
  Serial.println("speed saved");
#endif
}

// -------------------------------------------------------------------------------------
//  toggleSpeedMode()  -  enter or leave speed mode
// -------------------------------------------------------------------------------------
//  Leaving saves the speed first, so the flash write doesn't eat into the fast flash.
// -------------------------------------------------------------------------------------
void toggleSpeedMode() {
  speedMode = !speedMode;
  if (!speedMode) saveSpeed();
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
