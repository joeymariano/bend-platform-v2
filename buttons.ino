// =====================================================================================
//  buttons.ino  -  debounced button reading + a small non-blocking timer helper
// =====================================================================================
//
//  Buttons "bounce": as the contacts close they chatter open/closed for a few
//  milliseconds. Read raw, a single press could look like several presses. pressed()
//  only believes a reading once it has stayed the same for DEBOUNCE_MS (config.h).
//
//  Used by bends.ino (BTN-1 and BTN-2) and mode.ino (MODE).
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  pressed(b)
// -------------------------------------------------------------------------------------
//  Call this once per loop for each button.
//
//  Two jobs:
//    1. Updates b.down, the debounced "held right now" state. Momentary modes use it.
//    2. Returns true exactly once per press, at the moment the debounced state goes from
//       released to pressed. Latching modes and MODE use this.
//
//  It never returns true on release, and holding the button does not repeat.
// -------------------------------------------------------------------------------------
bool pressed(Button &b) {
  int r = digitalRead(b.pin);

  // Any change in the raw reading (a real press or just bounce) restarts the settle timer.
  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChange = millis();
  }

  bool edge = false;

  // Only act once the reading has stayed put long enough to be real. The unsigned
  // subtraction stays correct when millis() wraps around (after about 49 days).
  if (millis() - b.lastChange > DEBOUNCE_MS) {
    bool nowDown = (r == LOW);         // INPUT_PULLUP: LOW = pressed
    if (nowDown && !b.down) edge = true;   // released -> pressed = a new press
    b.down = nowDown;
  }

  return edge;
}

// -------------------------------------------------------------------------------------
//  elapsed(since, interval)
// -------------------------------------------------------------------------------------
//  A non-blocking "has `interval` ms passed since `since`?" check.
//  When it returns true it also resets `since` to now, so calling it every loop gives a
//  steady tick every `interval` ms. Used for the tremolo flips and the LED blink steps.
//
//  `since` is passed by reference, so each caller keeps its own timestamp.
// -------------------------------------------------------------------------------------
bool elapsed(unsigned long &since, unsigned long interval) {
  if (millis() - since >= interval) {  // wraparound-safe, same as above
    since = millis();
    return true;
  }
  return false;
}
