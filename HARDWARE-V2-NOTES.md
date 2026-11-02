# Bend Platform v2: firmware notes

Notes from the v2 PCB review (`bend Platform-v2.eprj`, Oct 2026).

## Hardware

| | |
|---|---|
| Bend switch | **CD4066BM96** analog switch (U2, LCSC C54755), control driven by an **AO3400A** (Q2/Q3) with a 10k pull-up to 9 V |
| LED interrupt | AO3400A (Q1): gate D1, drain → **LED-** pad → device's LED+, source → GND |
| Gate resistors | none |
| Gate pull-ups / pull-downs | none (see [Gate pins](#gate-pins-set-them-first-in-setup) below) |
| Power | 9 V → SS14 reverse-polarity diode → L78M05 → XIAO 5V pin |

Pin map: BTN-1 = D2, BTN-2 = D3, MODE = D9, Q1 = D1.

On the PCB, D5 → Q3 → CD4066 switch A → the **BND-1** pads, and D4 → Q2 → switch B → the
**BND-2** pads. The firmware's `bends[]` array is ordered to match, so BTN-1 works BND-1 and
BTN-2 works BND-2.

## ⚠️ Bend logic is inverted

The AO3400A pulls the CD4066 control pin low when its gate is high:

| Gate pin (D4/D5) | MOSFET | CD4066 control | Bend |
|---|---|---|---|
| **HIGH** | on | pulled to 0 V | **OPEN (bend off)** |
| **LOW** | off | pulled to 9 V by 10k | **CLOSED (bend on)** |

This is done in firmware. `BEND_ON` / `BEND_OFF` are LOW / HIGH in `config.h`,
`setBend()` uses them, and `setup()` starts both gates at `BEND_OFF` (HIGH).

The LED interrupt (Q1) is one wire: the **LED-** pad (Q1 drain) goes to the device's LED+
(anode). Q1's source is on the board's GND, so D1 HIGH pulls the anode to ground (LED dark)
and D1 LOW leaves the LED alone (LED on). The GND pad next to **LED-** is left unconnected.
After boot, D1 runs as TCC0 PWM at about 23 kHz (see `led.ino`), so the LED can be dimmed.
Don't `digitalWrite` D1 after that, because the pin belongs to the timer.

## Gate pins: set them first in `setup()`

There are no external gate pull resistors. The SAMD21 tri-states its pins while in reset and
keeps them floating until firmware configures them: at power-up, during the bootloader's ~0.5 s
window, and while flashing. During that window the bends and the LED glitch, and you can hear
it: a floating gate can sit in the AO3400A's threshold region, leaving the CD4066 control pin
part-way between 0 and 9 V, so the bend is part-on and noisy. Nothing is damaged; at most about
0.9 mA flows through the 10k pull-ups.

To keep the window short, `setup()` configures D1, D4 and D5 **before anything else**,
including `Serial.begin()`. It writes each level before switching the pin to OUTPUT.

Software can't reach the window before `setup()`. To silence it, hold each gate at its safe
level with a resistor (for a future board revision, or bodged onto the XIAO's pins now):

| Gate | Resistor | Holds it | Why |
|---|---|---|---|
| Q2/Q3 (D4/D5, bends) | ~100k gate → **3V3** (pull-up) | HIGH | bends off |
| Q1 (D1, LED) | ~100k gate → **GND** (pull-down) | LOW | Q1 off, LED lit as normal |

Don't pull Q2/Q3 to GND (gate-to-source): with the inverted bend logic, that holds both bends
**on** for the whole boot. These match `BEND_OFF` (HIGH) and `LED_GATE_AT_BOOT` (LOW) in
`config.h`.

## Watchdog

`watchdog.ino` turns on the SAMD21's hardware watchdog at the end of `setup()`, and `loop()`
feeds it. If the sketch hangs, the chip resets after about 2 s (`WDT_TIMEOUT` in `config.h`)
and starts like a fresh power-up, so the gates are set first again. With `LOG` on, the log
then says `reset by watchdog`.

## Using the bends (CD4066 limits, from the TI datasheet)

- The pedal being bent **must share ground** with the platform (the GND pad).
- Bend-point signals must stay **between 0 V and 9 V** relative to that ground. Signals outside
  that range get clipped, or can latch up the CD4066.
- At most **10 mA** through each switch. On-resistance is about 180–400 Ω at 9 V.

## Power cautions

- **Don't plug in USB while the 9 V battery is connected.** The XIAO's 5V pin is its USB rail,
  and USB would back-feed into the L78M05 output.
- A reversed battery is blocked by the SS14 (D1). The diode drops about 0.3 V, so the
  regulator stops regulating once the battery sags to about 7.4 V.
