// =====================================================================================
//  buttons.ino  -  button debouncing + a non-blocking timer
// =====================================================================================
//
//  A button's contacts chatter for a few ms when pressed, so one press can read as
//  several. readButton() acts on a change once it has held for BTN_CONFIRM_MS (2 ms,
//  enough to ignore a noise spike), then ignores the button for DEBOUNCE_MS while the
//  contacts settle. So a press registers within ~2 ms, not after the bounce is over.
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  readButtons()  -  stage 1 of loop(): sample every button once
// -------------------------------------------------------------------------------------
//  Stores each button's edge in .edge, so the rest of the loop works from one
//  snapshot of the inputs.
// -------------------------------------------------------------------------------------
void readButtons() {
  for (int i = 0; i < NUM_BENDS; i++) bends[i].button.edge = readButton(bends[i].button);
  modeBtn.edge = readButton(modeBtn);
}

// -------------------------------------------------------------------------------------
//  readButton(b)  -  read one button (called by readButtons)
// -------------------------------------------------------------------------------------
//  Updates b.down (held right now) and returns the edge, once:
//    BTN_PRESS     released -> pressed
//    BTN_RELEASE   pressed -> released
//    BTN_NONE      no change
// -------------------------------------------------------------------------------------
ButtonEdge readButton(Button &b) {
  int r = digitalRead(b.pin);

  // Any raw change, real or bounce, restarts the confirm timer.
  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChange = millis();
  }

  bool nowDown = (r == LOW);         // LOW = pressed
  if (nowDown == b.down) return BTN_NONE;

  // (Unsigned subtraction survives millis() wrapping.)
  if (millis() - b.lastChange < BTN_CONFIRM_MS) return BTN_NONE;   // not held long enough yet
  if (millis() - b.lastEdge < DEBOUNCE_MS)      return BTN_NONE;   // still bouncing from the last edge

  b.down = nowDown;
  b.lastEdge = millis();
  if (nowDown) b.downSince = b.lastChange;   // when the contacts closed
  return nowDown ? BTN_PRESS : BTN_RELEASE;
}

// -------------------------------------------------------------------------------------
//  pressLength(b)  -  how long the button has been down so far (ms)
// -------------------------------------------------------------------------------------
//  After BTN_RELEASE: how long the press lasted. lastEdge is then the moment the
//  release counted, and downSince the moment the contacts closed.
// -------------------------------------------------------------------------------------
unsigned long pressLength(const Button &b) {
  return (b.down ? millis() : b.lastEdge) - b.downSince;
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
