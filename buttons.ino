// =====================================================================================
//  buttons.ino  -  button debouncing + a non-blocking timer
// =====================================================================================
//
//  A button's contacts chatter for a few ms when pressed, so one press can read as
//  several. pressed() only trusts a reading that has held for DEBOUNCE_MS.
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  pressed(b)  -  call once per loop for each button
// -------------------------------------------------------------------------------------
//  Updates b.down (held right now) and returns true once per press, when the button
//  goes from released to pressed. Releases and holds never return true.
// -------------------------------------------------------------------------------------
bool pressed(Button &b) {
  int r = digitalRead(b.pin);

  // Any change, real or bounce, restarts the settle timer.
  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChange = millis();
  }

  bool edge = false;

  // Trust the reading once it has held. (Unsigned subtraction survives millis() wrapping.)
  if (millis() - b.lastChange > DEBOUNCE_MS) {
    bool nowDown = (r == LOW);         // LOW = pressed
    if (nowDown && !b.down) edge = true;   // released -> pressed
    b.down = nowDown;
  }

  return edge;
}

// -------------------------------------------------------------------------------------
//  elapsed(since, interval)
// -------------------------------------------------------------------------------------
//  Returns true once `interval` ms have passed since `since`, and resets `since` to now.
//  Called every loop, it ticks every `interval` ms. Each caller keeps its own `since`.
// -------------------------------------------------------------------------------------
bool elapsed(unsigned long &since, unsigned long interval) {
  if (millis() - since >= interval) {  // wrap-safe
    since = millis();
    return true;
  }
  return false;
}
