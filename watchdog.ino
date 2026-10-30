// =====================================================================================
//  watchdog.ino  -  reset the board if the sketch ever hangs
// =====================================================================================
//
//  The SAMD21's watchdog is a hardware countdown. loop() feeds it every pass. If loop()
//  ever stops running (a crash, a lockup, a glitch after hours of being left on), the
//  countdown runs out after about WDT_TIMEOUT and the chip resets itself. It comes back
//  like a fresh power-up: bends off, mode 1, mode blink.
//
//  The watchdog runs from its own slow oscillator (OSCULP32K), so it keeps counting even
//  if the main 48 MHz clock is what failed.
//
// =====================================================================================

// -------------------------------------------------------------------------------------
//  watchdogBegin()  -  call once, at the end of setup()
// -------------------------------------------------------------------------------------
//  Clock: OSCULP32K (32,768 Hz) / 32 = 1,024 Hz, on generic clock 2 (the Arduino core
//  leaves clock 2 for the watchdog). WDT_TIMEOUT counts those ticks.
// -------------------------------------------------------------------------------------
void watchdogBegin() {
  // Clock generator 2: OSCULP32K divided by 2^(4+1) = 32.
  GCLK->GENDIV.reg = GCLK_GENDIV_ID(2) | GCLK_GENDIV_DIV(4);
  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_GENEN |
                      GCLK_GENCTRL_SRC_OSCULP32K | GCLK_GENCTRL_DIVSEL;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Feed clock 2 to the watchdog.
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_WDT | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Stop it while configuring (it's off after reset, but be sure).
  WDT->CTRL.reg = 0;
  while (WDT->STATUS.bit.SYNCBUSY);

  // Plain timeout: no early-warning interrupt, no window mode.
  WDT->INTENCLR.reg = WDT_INTENCLR_EW;
  WDT->CONFIG.reg = WDT_CONFIG_PER(WDT_TIMEOUT);
  while (WDT->STATUS.bit.SYNCBUSY);

  WDT->CTRL.reg = WDT_CTRL_ENABLE;
  while (WDT->STATUS.bit.SYNCBUSY);
}

// -------------------------------------------------------------------------------------
//  watchdogFeed()  -  call once per loop
// -------------------------------------------------------------------------------------
//  Restarts the countdown. Writing CLEAR while the previous one is still syncing would
//  stall the CPU, so skip it then; loop() comes back around in microseconds.
// -------------------------------------------------------------------------------------
void watchdogFeed() {
  if (!WDT->STATUS.bit.SYNCBUSY) WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
}

// -------------------------------------------------------------------------------------
//  logResetCause()  -  logs "reset by watchdog" if that's why we just started
// -------------------------------------------------------------------------------------
//  Does nothing when LOG is 0.
// -------------------------------------------------------------------------------------
void logResetCause() {
#if LOG
  if (PM->RCAUSE.reg & PM_RCAUSE_WDT) Serial.println("reset by watchdog");
#endif
}
