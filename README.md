# Bend Platform v2

<p align="center">
  <img src="docs/layouts-schematics/3D_PCB1_2026-10-15.png" alt="Bend Platform v2 PCB render" width="95%">
</p>
<p align="center">
  <img src="docs/final-mod%2Bwiring/mod.jpeg" alt="Internal mod example" width="95%">
</p>
<p align="center">
  <img src="docs/layouts-schematics/SCH_Schematic1_1-P1_2026-10-14.png" alt="Schematic" height="360">
  <img src="docs/final-mod%2Bwiring/top-angle.jpeg" alt="Pedal example" height="360">
</p>

Adds physical buttons to a device (a guitar fuzz/distortion pedal, for
example) that open and close circuit "bends" in a musical way. A Seeeduino XIAO
(SAMD21) on the **Bend Platform v2** PCB drives three **AO3400A** MOSFETs. One
interrupts the device's main LED, which is used as the mode indicator. The other two
drive a **CD4066** analog switch that toggles or stutters two circuit-bend points on
the device.

See [`HARDWARE-V2-NOTES.md`](HARDWARE-V2-NOTES.md) for the v2 board review notes: bend
polarity, CD4066 limits, and power cautions.

## Prototype platform

Prototyped against a **LANDTONE DIY "Build Your Own Fuzz Pedal" kit** — a through-hole,
true-bypass fuzz/distortion in a 1590B enclosure (Volume + Drive controls, 9 V).
See [`docs/parts-data/landtone-kit/`](docs/parts-data/landtone-kit/) for kit details and
where the controller taps in; product listing:
[Amazon B06XQL49G8](https://www.amazon.com/dp/B06XQL49G8).

## Controls

- **MODE button**
  - **Short press** (acts on release): cycles through the five bend modes
    (1 → 2 → 3 → 4 → 5 →) and blinks the device's main LED the mode number (1–5) so you
    can tell which mode you're in.
  - **Long press** (hold 2 s; it acts at the 2 s mark, no need to let go): enters or
    leaves [speed mode](#speed-mode). Entering works only in the modes with a clock (2, 4
    and 5)
- **BTN-1 / BTN-2** — work the two bends according to the current mode.

### Main LED

The device's own LED doubles as a status light:

- **Waiting:** dim (about half brightness, set by `LED_IDLE_LEVEL`).
- **Momentary / latching / sequencer:** full brightness while either bend is engaged, so
  in mode 5 the LED follows the pattern.
- **Momentary-trem / latching-trem:** while a bend stutters, the LED does a soft,
  sine-rounded pulse once per stutter cycle (25% wide), locked to the stutter. Both bends
  share one stutter clock, so they always flip together.
- **MODE press:** dark blinks, one per mode number.
- **Speed mode:** a full dark-to-bright sawtooth, twice: ramping up going in, ramping
  down coming out, so it can't be mistaken for a mode blink.

### Modes

| # | Mode              | Bend button behavior                         |
|---|-------------------|----------------------------------------------|
| 1 | momentary         | bend engaged while the button is held        |
| 2 | momentary tremolo | bend stutters while the button is held       |
| 3 | latching          | press toggles the bend on/off                |
| 4 | latching tremolo  | press toggles a stuttering bend on/off       |
| 5 | chaos sequencer   | BTN-1 starts/stops a looping pattern across both bends; BTN-2 plays it while held |

Mode 5's default pattern, one step every 150 ms (`SEQ_STEP_MS`):

```
step:    1      2      3        4
bend 1   .      .      ON       .
bend 2   .      ON     ON       ON
```

It's an example: see [Make it yours](#make-it-yours).

### Speed mode

Sets how fast the stutter runs in both trem modes, and how fast the sequencer steps,
with the LED as a guide.

1. In mode 2, 4 or 5, hold MODE for 2 s. The LED ramps up twice, then pulses at the current
   speed. (In modes 1 and 3 the long press does nothing: they have no clock.)
2. Press and release **BTN-1** to slow it down, or **BTN-2** to speed it up. Each press is
   10%, from about 0.26x to 3.8x. The bends keep running, so you hear each change. In
   modes 4 and 5 a running bend or pattern keeps running: presses here set the speed,
   they don't toggle. In mode 2 a held button still stutters its bend.
3. Hold MODE for 2 s again. The LED ramps down twice and normal play resumes at the new
   speed.

The pulse shows the rate of the current mode: momentary-trem's in mode 2, latching-trem's
in mode 4, the step rate (one pulse per two steps) in mode 5. One speed setting covers all
three. Short MODE presses do nothing in speed mode.

If you don't touch a button for 30 s (`SPEED_TIMEOUT_MS`), it leaves speed mode on its
own, just like the long press: the speed is saved and the LED ramps down twice. The
stutter keeps its rhythm through the exit, so it's safe mid-song. Holding a bend counts
as touching a button.

The speed is saved to the SAMD21's flash when you leave speed mode (only if it changed),
and loaded at power-up. The bend mode is saved too, 3 s after your last short MODE press,
so it comes back in the mode you left it. Uploading a new sketch erases both, back to
mode 1 at 1.0x.

## Pin map (Seeeduino XIAO SAMD21, Bend Platform v2 PCB)

| Signal            | Pin | Drives / net                          | Notes                                  |
|-------------------|-----|---------------------------------------|----------------------------------------|
| MODE button       | D9  | MODE                                  | `INPUT_PULLUP`, to GND                  |
| BTN-1             | D2  | BTN-1                                 | `INPUT_PULLUP`, to GND                  |
| BTN-2             | D3  | BTN-2                                 | `INPUT_PULLUP`, to GND                  |
| Q1 gate           | D1  | LED- pad (Q1 drain) → device's LED+   | interrupts the device's main LED        |
| Q3 gate           | D5  | CD4066 CONTROL A → BND-1 / BND-1-OUT  | bend 1 (BTN-1); **active LOW**          |
| Q2 gate           | D4  | CD4066 CONTROL B → BND-2 / BND-2-OUT  | bend 2 (BTN-2); **active LOW**          |

The gates are driven directly, with no series resistors and no external pull-downs.
The firmware sets the gate pins first thing in `setup()`. The buttons pull to GND.

## Hardware wiring notes

### Bends (CD4066)

Each bend is a CD4066 switch. Q2 and Q3 pull its control pin low against a 10k pull-up
to 9 V, so the logic is inverted: **gate HIGH = bend open, gate LOW = bend closed**.
The firmware handles this with `BEND_ON` / `BEND_OFF` in `config.h`.

From the TI CD4066B datasheet:

- The device being bent **must share ground** with the platform (the GND pad).
- Bend-point signals must stay **between 0 V and 9 V** relative to that ground.
- At most **10 mA** through each switch. On-resistance is about 180–400 Ω at 9 V.

### LED interrupt (Q1)

The LED hack is **one wire**: run the **LED-** pad (Q1's drain) to the device's
**LED+** (the anode, after its resistor). Leave the GND pad next to LED- unconnected.
Q1's source is already on the platform's GND on the PCB.

When D1 goes HIGH, Q1 pulls the LED's anode to ground and the LED goes dark. When D1 is
LOW, Q1 is out of the way and the LED behaves normally, still controlled by the device's
own switch. This relies on the device sharing ground with the platform, which it must
anyway for the bends.

D1 is driven by the SAMD21's TCC0 timer as PWM at about 23 kHz, above hearing, so dimming
the LED doesn't put a whine into the audio. (Arduino's `analogWrite()` would run at about
732 Hz.) The more of each cycle Q1 is on, the dimmer the LED. All the levels and timings
are in `config.h`.

### Power

9 V battery → SS14 reverse-polarity diode → L78M05 → the XIAO's 5V pin.

- A reversed battery is blocked by the diode.
- The regulator drops out once the battery sags to about 7.4 V.
- **Don't plug in USB while the battery is connected.** USB would back-feed into the
  regulator output.

## Serial log

Connect at **9600 baud** for an event log of what each press does, e.g.:

```
MODE -> mode 3 (latching)
BTN-1 -> bend1 latched ON
SPEED mode ON, 1.00x
BTN-2 -> faster, 1.10x
settings saved
SPEED mode OFF, 1.10x
```

At power-up the log shows the speed it loaded, e.g. `speed 1.10x (saved)` or
`speed 1.00x (default)`.

After a watchdog reset the log starts with `reset by watchdog`.

Set `#define LOG 0` in `config.h` to compile it out.

## Watchdog

`loop()` feeds the SAMD21's hardware watchdog on every pass. If the sketch ever hangs
(say, after the device is left on for a long time), the watchdog resets the board after
about 2 s (`WDT_TIMEOUT` in `config.h`). It comes back like a fresh power-up: bends off,
the saved mode and speed, one mode blink.

## Code layout

The sketch is split into Arduino IDE tabs:

| File | What's in it |
|---|---|
| `bend-platform-v2.ino` | Overview, global state, `setup()`, `loop()` |
| `config.h` | Pins, bend/LED polarity, timings, `LOG` flag, types. Settings live here. |
| `buttons.ino` | Reads every button once per loop (debounced), `elapsed()` timer helper |
| `bends.ino` | Drives the two bends in modes 1–4 |
| `sequence.ino` | Mode 5: the step sequencer, and a template for your own mode |
| `mode.ino` | MODE button: short press steps the modes, long press toggles speed mode |
| `speed.ino` | Speed mode: BTN-1 / BTN-2 set the stutter / step speed |
| `flash.ino` | Saves settings (speed and mode) to flash between power-ups |
| `led.ino` | Main LED: ultrasonic PWM, mode blinks, speed-mode sweeps, bend animation |
| `watchdog.ino` | Hardware watchdog: resets the board if the sketch hangs |

Each pass of `loop()` runs in five stages: read every button, decide (mode and speed
mode), drive the bends, drive the LED, then housekeeping (save settings, feed the
watchdog).

## Make it yours

This board is a platform. Your bends and your modes are what make the instrument yours,
so go and find them.

### Program your own mode

Mode 5 is there to be copied. To start, edit `SEQ_PATTERN` in `config.h`: one row per
step, `{ bend 1, bend 2 }`, as many rows as you like. Change `SEQ_STEP_MS` for the tempo.
Odd lengths, long silences, a rhythm from a song you like: try things.

To add a whole new mode (mode 6 and up):

1. Add it to `enum Mode` and `MODE_NAMES` in `config.h`. It gets the next number, and
   the LED blinks that many times when you select it.
2. Write a service function for it in a new tab, like `serviceSequence()` in
   `sequence.ino`. It gets this loop's button edges. Set the bends only through
   `setBend()`, so the LED and the shared clock stay in sync. `clockTicks()` gives you the
   shared clock, and speed mode can scale it (add your mode to `usesClock()` in
   `mode.ino`).
3. Call it from `serviceBends()` in `bends.ino`.

Some ideas: random steps, a pattern that speeds up while you hold a button, BTN-1 and
BTN-2 playing two different patterns, a bend that fades in by stuttering faster and
faster. If you make something good, share it.

### Find your own bends

A bend is two points on the device's circuit that sound interesting when joined (or,
with a cut trace, when reconnected). The platform's CD4066 just closes that connection
for you. [`docs/bend-locations/`](docs/bend-locations/) shows the ones used on the fuzz.
To find your own:

- Use **battery-powered devices only**. Never bend anything that plugs into mains.
- With the device powered and playing, touch the two ends of a jumper wire (or a
  1k–10k resistor, or a pot, for a gentler effect) between points on the board and
  listen. Transistor legs, capacitor legs, IC pins and the feedback path between an
  op-amp's output and input are good places to start.
- Mark the ones you like. Keep the ones that make sound; skip any that kill it, get hot
  or make the device reset.
- Before wiring a point to the platform, check the [CD4066 limits](#bends-cd4066): it
  must share ground with the platform, and the signal must stay between 0 V and 9 V, at
  most 10 mA.
- A series resistor or pot between the bend point and the platform's BND pads tames a
  bend that's too harsh.

## Build

Arduino IDE or `arduino-cli`, board **Seeeduino XIAO (SAMD21)**:

```
arduino-cli compile --fqbn Seeeduino:samd:seeed_XIAO_m0 .
arduino-cli upload  --fqbn Seeeduino:samd:seeed_XIAO_m0 -p <port> .
```

If upload fails to find the board, double-tap RESET to enter the bootloader, then
reselect the port.

## Repo contents

- `*.ino`, `config.h` — the firmware (see Code layout above)
- `HARDWARE-V2-NOTES.md` — Bend Platform v2 board review and firmware notes
- `docs/layouts-schematics/` — v2 schematic, 3D render, Gerbers, BOM and pick-and-place.
  The EasyEDA Pro project (`bend Platform-v2.eprj`) lives in the EasyEDA Pro projects
  folder.
- `docs/drill-template/` — enclosure / faceplate layout SVGs
- `docs/bend-locations/` — annotated photos of the bend points on the fuzz
- `docs/parts-data/chip/` — CD4066B datasheet
- `docs/parts-data/micro-controller/` — SAMD21 datasheet
- `docs/parts-data/transistors/` — AO3400A datasheet
- `docs/parts-data/landtone-kit/` — notes on the LANDTONE fuzz kit used to prototype
- `docs/prototype-wiring/` — build photos
- `docs/art/` — sticker artwork
