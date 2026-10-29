# Prototype platform — LANDTONE DIY Fuzz kit

This circuit-bent controller is being prototyped against a **LANDTONE "Build Your Own
Fuzz Pedal" DIY kit** in a 1590B-style aluminum enclosure.

Product: [Build Your Own Fuzz Pedal — 1590B Aluminum Stomp Box (B06XQL49G8)](https://www.amazon.com/dp/B06XQL49G8)
(LANDTONE sells several near-identical fuzz/distortion kits across Amazon, Reverb and eBay.)

## Kit specs

- DIY **fuzz / distortion** pedal, op-amp based, **through-hole** parts
- **1590B**-style aluminum enclosure
- **3PDT footswitch, true bypass**
- Runs on a **9 V battery** or a standard **negative-tip 9 V** supply
- Numbered PCB (each part has a matching number on the board)
- Controls: **Volume (VOL)** and **Drive (DRV)** — see the enclosure SVGs in `docs/drill-template/`

## How this project ties in

The Bend Platform v2 board taps three points on the assembled pedal:

- **Q1 / LED-** → one wire to the pedal's **LED+** (anode). Q1 dims or darkens the
  main LED, used here as a status light.
- **BND-1** and **BND-2** → two circuit-**bend** points on the fuzz PCB, switched by the
  CD4066.

See the repo-root `README.md` for the pin map and wiring notes, and
`Landtone-Fuzz-Wiring.jpg` in this folder for how the stock pedal is wired.

## Photos

Build photos are in `docs/prototype-wiring/`.
`Landtone-Fuzz-Wiring.jpg` here is a stock wiring reference for the kit.
