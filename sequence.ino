// =====================================================================================
//  sequence.ino  -  mode 5: a step sequencer for the two bends
// =====================================================================================
//
//  Plays SEQ_PATTERN (config.h) in a loop, one step every SEQ_STEP_MS. The default:
//
//      step:    1      2      3        4
//      bend 1   .      .      ON       .
//      bend 2   .      ON     ON       ON
//
//    BTN-1   starts / stops the pattern (acts on contact, like latching)
//    BTN-2   plays it while held (like momentary)
//
//  It always starts from step 1. Speed mode works here too: it scales the step time.
//
//  This tab is meant as a starting point for your own mode. To add one:
//    1. add it to `enum Mode` and MODE_NAMES (config.h). It gets the next number,
//       and blinks that many times.
//    2. write a service function like serviceSequence() below. Set the bends only
//       through setBend(), so the LED and the stutter clock stay in step.
//    3. call it from serviceBends() (bends.ino).
//
// =====================================================================================

bool seqLatched = false;   // BTN-1 has the pattern playing

// -------------------------------------------------------------------------------------
//  serviceSequence(edges)  -  drive both bends from the pattern (called by serviceBends)
// -------------------------------------------------------------------------------------
//  edges: this loop's button edges (BTN_NONE in speed mode, so presses don't toggle).
// -------------------------------------------------------------------------------------
void serviceSequence(const ButtonEdge edges[]) {
  if (edges[0] == BTN_PRESS) {
    seqLatched = !seqLatched;
#if LOG
    Serial.println(seqLatched ? "BTN-1 -> sequence ON" : "BTN-1 -> sequence OFF");
#endif
  }

  bool playing = seqLatched || bends[1].button.down;
  int step = playing ? clockTicks() % SEQ_LEN : 0;   // the shared clock: step 1 first

  // A playing bend follows the clock even on its off steps (stuttering = true), so the
  // clock keeps running between hits.
  for (int i = 0; i < NUM_BENDS; i++) {
    setBend(bends[i], playing && SEQ_PATTERN[step][i], playing);
  }
}
