// =====================================================================================
//  buttons.ino  -  button debouncing + a non-blocking timer
// =====================================================================================
//
//  A button's contacts chatter for a few ms when pressed, so one press can read as
//  several. readButton() only trusts a reading that has held for DEBOUNCE_MS.
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  readButton(b)  -  call once per loop for each button
// -------------------------------------------------------------------------------------
//  Updates b.down (held right now) and returns the edge, once:
//    BTN_PRESS     released -> pressed
//    BTN_RELEASE   pressed -> released
//    BTN_NONE      no change
// -------------------------------------------------------------------------------------
ButtonEdge readButton(Button &b) {
  int r = digitalRead(b.pin);

  // Any change, real or bounce, restarts the settle timer.
  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChange = millis();
  }

  // Trust the reading once it has held. (Unsigned subtraction survives millis() wrapping.)
  if (millis() - b.lastChange <= DEBOUNCE_MS) return BTN_NONE;

  bool nowDown = (r == LOW);         // LOW = pressed
  if (nowDown == b.down) return BTN_NONE;

  b.down = nowDown;
  if (nowDown) b.downSince = b.lastChange;   // when the contacts closed
  return nowDown ? BTN_PRESS : BTN_RELEASE;
}

// -------------------------------------------------------------------------------------
//  pressLength(b)  -  how long the press lasted (ms)
// -------------------------------------------------------------------------------------
//  Use it in the loop that got BTN_RELEASE. lastChange is then the moment the contacts
//  opened, and downSince the moment they closed.
// -------------------------------------------------------------------------------------
unsigned long pressLength(const Button &b) {
  return b.lastChange - b.downSince;
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
