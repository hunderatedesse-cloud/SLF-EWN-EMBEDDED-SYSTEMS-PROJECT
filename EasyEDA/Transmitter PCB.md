---
tags:
  - slf-ewn
  - easyeda
  - transmitter
  - pcb
parent: "[[EasyEDA Hub]]"
---

# Transmitter PCB

← [[EasyEDA Hub]] · **Full netlist:** [[EasyEDA Hub#Transmitter — definitive connections]] · Firmware: [[Firmware Hub]] · Sim: [[Transmitter Simulation]] · Capsule: [[Mechanical Hub]]

| Spec | Value |
|:-----|:------|
| Size | **50 × 50 mm**, 1-layer THT |
| MCU | ATmega328P-PU in DIP-28 socket, 8 MHz RC |
| Power | 3.6 V Li-SOCl₂ + **1.2 kΩ** + **47 mF / 5.5 V** supercap (TX only) |

**Schematic** · **PCB** · **3D**

![[Schematic transmitter.png]]
*Figure T1 — transmitter schematic export, Rev 1.0. Pre-fab; symbols show the LIS3DH-era footprint in places (H7).*

![[PCB_PCB_SingleLayer_SQUARE_2026-08-26.png]]
*Figure T2 — transmitter 50 × 50 mm single-layer layout, Rev 1.0.*

![[Attachments/transmitter 3D PCB.png]]
*Figure T3 — transmitter 3D top view, Rev 1.0.*

![[Attachments/transmitter 3D PCB back.png]]
*Figure T4 — transmitter 3D bottom view, Rev 1.0.*

## Parts on this board

| Function | Part | EasyEDA | Mount | Notes |
|:---------|:-----|:--------|:------|:------|
| MCU | ATmega328P-PU | `ATMEGA328P-PU` | DIP-28 socket | Always on 3.6 V |
| Radio | **Ra-02** (SX1278 433 MHz) | `Ra-02` | Female 2.54 mm | **VCC_SW** (FET-gated) |
| Accel | **ADXL345** | `ADXL345` | Header | **Always on 3.6 V** (motion INT in sleep) |
| Moisture | Capacitive v1.2 | `Capacitive Soil Moisture` | Edge pads | **VCC_SW** (FET-gated) |
| FET | ZVP2106A | `ZVP2106A` | TO-92 | High-side switch for VCC_SW |
| Series R | **1.2 kΩ** | — | Cell → always-on rail | Cell inrush limit 3.0 mA; load peaks come from the supercap |
| Energy buffer | **47 mF / 5.5 V supercap** | — | After R, to GND | **TX only** — PA peak |
| Cell | Li-SOCl₂ AA ~2400 mAh | — | Holder / wires | Primary energy |
| Antenna | SMA-TH + helix | `SMA-TH` | Board edge | Air-pocket helix; Ra-02 u.FL → SMA via short pigtail |
| Disconnect | 2-pin header + shunt | — | Master power | Storage isolation |

### Power architecture (important)

```
Li-SOCl₂ ── 1.2 kΩ ──┬── 47 mF / 5.5 V supercap ── GND
                     ├── MCU VCC / AVCC (always)
                     ├── ADXL345 VCC (always — low-power INT mode)
                     └── FET source
                         FET drain = VCC_SW
                               ├── Moisture sensor VCC
                               └── Ra-02 VCC
Gate PD4 + 100 kΩ pull-up to 3.6 V (OFF during reset)

ΔV ≈ 127 mV @ 120 mA × 50 ms; I_max = 3.0 mA; τ = 56.4 s; 5τ ≈ 4.7 min ≪ 15 min
```

| Rail | Loads | Why |
|:-----|:------|:----|
| **Always-on 3.6 V** | MCU, ADXL345 | Sleep + motion interrupt must work without waiting for FET |
| **VCC_SW (FET)** | Moisture, Ra-02 only | Kill quiescent draw between samples |
| **Supercap** | Parallel to cell | Feed ~120 mA LoRa PA edge the cell cannot source cleanly |

Do **not** put ADXL345 on VCC_SW if emergency INT wake is required during the 15 min sleep window.

Must match `transmitter.c` (DIP-28):

| MCU pin | Net | Goes to |
|:--------|:----|:--------|
| 19 PB5 | SCK | Ra-02 SCK, ADXL345 SCLK |
| 17 PB3 | MOSI | Ra-02 MOSI, ADXL345 MOSI |
| 18 PB4 | MISO | Ra-02 MISO, ADXL345 MISO |
| 15 PB1 | NSS | Ra-02 NSS |
| 16 PB2 | CS | ADXL345 CS |
| 6 PD4 | gate | ZVP2106A |
| 4 PD2 | INT0 | ADXL345 INT1 |
| 5 PD3 | INT1 | Ra-02 DIO0 |
| 24 PC1 | ADC1 | Moisture SIG |
| 3 PD1 | TXD | debug header 9600 (optional) |
| 7 / 8 | VCC / GND | 3.6 V · 20 AVCC |

## Recommended passives (TX)

| Part | Value | Where |
|:-----|:------|:------|
| Gate pull-up | **100 kΩ** | PD4 / FET gate → 3.6 V |
| CS pull-ups | **100 kΩ** | Ra-02 NSS, ADXL345 CS → 3.6 V |
| INT pull-down (if open-drain INT) | **10–100 kΩ** | INT0 → GND if module requires |
| Decouple MCU | **0.1 µF** ×2 | within 5 mm of VCC and AVCC |
| Decouple ADXL | **0.1 µF** | at module VCC (always-on rail) |
| Decouple radio | **0.1 µF + 10–100 µF** | at Ra-02 VCC on **VCC_SW** |
| Decouple moisture | **0.1 µF** | at probe VCC on **VCC_SW** |
| Series R | **1.2 kΩ** | between cell/jumper and always-on rail |
| Supercap | **47 mF / 5.5 V** | always-on rail to GND (polarity!) |
| RESET | **10 kΩ** | RESET → 3.6 V |

**Optional (nice, not mandatory for prototype)**

| Part | Why |
|:-----|:----|
| Series **1–10 Ω** + bulk at radio | Soften PA edge slightly |
| Schottky in series with cell | Reverse-polarity protection if holder can be inserted wrong |
| TVS on antenna (low-cap) | ESD; optional on student build |

Short, straight ANT → SMA **central pin**; chassis legs to GND. SMA search: `SMA-TH` or `SMA-JW-P` (90° preferred).

Helix in an **air pocket**: cut **17.0 cm**, 5 mm mandrel, **10–11 close turns at ~3.2 mm pitch**, ~3.4 cm. Math: [[Project Overview#Radio]].

**Stack inside the tube (top → soil):**

```
SMA + air for helix
Ra-02 on female 2.54 mm          ← VCC_SW
ATmega328P in DIP-28
ADXL345 + ZVP2106A + ceramics    ← ADXL on always-on 3.6 V
47 mF / 5.5 V supercap
Li-SOCl₂ AA
Moisture blade through bottom    ← VCC_SW
```

Reserve **unpopulated** pads for a future digital extensometer — do not fit that connector in this build.

**BOM (this board):** MCU + socket, Ra-02, ADXL345, moisture probe, ZVP2106A, Li-SOCl₂, **supercap**, SMA, headers, passives. **~$27.** Totals: [[Project Overview#Bill of materials]].

**Not on TX:** USB, FT232, crystal (RC clock), W25Q64, LM1117.
