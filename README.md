# Bend Platform v2

![Bend Platform v2 PCB render](docs/layouts-schematics/3D_PCB1_2026-10-15.png)

Adds physical buttons to a circuit-bent device (a guitar fuzz/distortion pedal, for
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

- **MODE button** — cycles through the four bend modes (1 → 2 → 3 → 4 →) and blinks the
  device's main LED the mode number (1–4) so you can tell which mode you're in.
- **BTN-1 / BTN-2** — work the two bends according to the current mode.

### Main LED

The device's own LED doubles as a status light:

- **Waiting:** dim (about half brightness, set by `LED_IDLE_LEVEL`).
- **Momentary / latching:** full brightness while either bend is engaged.
- **Momentary-trem / latching-trem:** while a bend stutters, the LED does a soft,
  sine-rounded pulse once per stutter cycle (25% wide), locked to the stutter. Both bends
  share one stutter clock, so they always flip together.
- **MODE press:** dark blinks, one per mode number. These win over the animation.

### Modes

| # | Mode              | Bend button behavior                         |
|---|-------------------|----------------------------------------------|
| 1 | momentary         | bend engaged while the button is held        |
| 2 | momentary tremolo | bend stutters while the button is held       |
| 3 | latching          | press toggles the bend on/off                |
| 4 | latching tremolo  | press toggles a stuttering bend on/off       |

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
```

After a watchdog reset the log starts with `reset by watchdog`.

Set `#define LOG 0` in `config.h` to compile it out.

## Watchdog

`loop()` feeds the SAMD21's hardware watchdog on every pass. If the sketch ever hangs
(say, after the device is left on for a long time), the watchdog resets the board after
about 2 s (`WDT_TIMEOUT` in `config.h`). It comes back like a fresh power-up: bends off,
mode 1, one mode blink.

## Code layout

The sketch is split into Arduino IDE tabs:

| File | What's in it |
|---|---|
| `bend-platform-v2.ino` | Overview, global state, `setup()`, `loop()` |
| `config.h` | Pins, bend/LED polarity, timings, `LOG` flag, types. Settings live here. |
| `buttons.ino` | Debounced button reading, `elapsed()` timer helper |
| `bends.ino` | Drives the two bends in each of the four modes |
| `mode.ino` | MODE button: steps through the modes |
| `led.ino` | Main LED: ultrasonic PWM, mode blink, bend animation |
| `watchdog.ino` | Hardware watchdog: resets the board if the sketch hangs |

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
