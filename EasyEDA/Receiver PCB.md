---
tags:
  - slf-ewn
  - easyeda
  - receiver
  - pcb
parent: "[[EasyEDA Hub]]"
---

# Receiver PCB

← [[EasyEDA Hub]] · **Full netlist:** [[EasyEDA Hub#Receiver — definitive connections]] · Sim: [[Receiver Simulation]] · Firmware: [[Firmware Hub]]

| Spec | Value |
|:-----|:------|
| Size | **95 × 70 mm**, 1-layer THT |
| MCU | ATmega328P-PU socket, **16 MHz** crystal |
| Power | USB 5 V **or** 5.5–9 V battery header → LM1117-3.3 |
| Log | W25Q64 8 MB SPI NOR |

Older drafts used MicroSD or two Li-SOCl₂ cells. The build is **USB 5 V** plus a **2-pin rechargeable-pack header** (solar-charged), so logging continues when USB is unplugged. Storage is **W25Q64**, not a card.

## What does **not** go on the receiver

| Part | Why absent |
|:-----|:-----------|
| **47 mF + 1.2 kΩ (TX RC)** | USB can source radio/MCU current; RC buffer is for **buried primary-cell TX peaks only** |
| **ZVP2106A / VCC_SW** | Tower stays powered for continuous RX; no multi-year µA sleep requirement |
| **ADXL345 / moisture** | Sensing is on the node |
| **Li-SOCl₂ as sole supply** | Tower now runs from a **rechargeable pack** on the header; a thionyl primary cell is not used |

**Schematic** · **PCB** · **3D**

![[Schematic_landslideReciever_2026-08-26.png]]
*Figure R1 — receiver schematic export, Rev 1.0. Pre-fab; the image still shows the dual-cell / supercap / EEPROM revision (H7).*

![[PCB_PCB_landslideReciever_2026-08-26.png]]
*Figure R2 — receiver 95 × 70 mm single-layer layout, Rev 1.0.*

![[Attachments/Receiver 3D PCB.png]]
*Figure R3 — receiver 3D top view, Rev 1.0.*

![[Attachments/Receiver 3D PCB back.png]]
*Figure R4 — receiver 3D bottom view, Rev 1.0.*

> [!warning] Images are not as-built (H7)
> The exported revision shows two Li-SOCl₂ holders, a supercap, and an EEPROM footprint. The build text and `Code/` specify **USB + battery-header power** and a **W25Q64** log. Re-export both boards after the schematic is reconciled — see **H7** on [[Status and open issues]].

## Battery and solar input

The receiver logs on its own; USB is only the dump/debug path. To keep logging when the laptop is disconnected, the board carries a **2-pin battery inlet** and runs from the pack until USB returns.

| Item | Value |
|:-----|:------|
| Header | **2-pin 5.08 mm** screw terminal (JST-PH 2.0 mm for a sealed pack), pin 1 **BATT+**, pin 2 **BATT−** |
| Input window | **5.5–9.0 V** at the header |
| Protection | **1 A polyfuse** in BATT+, series **SS34** reverse-polarity/OR diode |
| Source OR | **USB 5 V → SS34 → LM1117 IN** and **BATT → fuse → SS34 → LM1117 IN**; the higher source wins |
| Packs that fit | 6 V SLA (5.7–7.2 V) · 2S LiFePO4 (5.5–7.3 V) · 2S Li-ion (6.0–8.4 V) with BMS |
| Charging | **External**: solar panel → 6/12 V PWM controller → pack; the board does not charge or balance |
| Low-voltage cut-off | Controller load output or pack BMS; **no LVD on the board** |

**Budget (battery mode, FT232 off):** Ra-02 RX ≈11 mA + MCU ≈9 mA + LM1117 quiescent ≈5 mA → **≈25 mA at 3.3 V**, **≈30 mA from a 6 V pack**, **≈4.3 Wh/day**. A 6 V 4.5 Ah SLA (27 Wh) gives **≈6 days without sun**; a 6 V 5 W panel at ~3 peak-sun hours and 60 % derating gives **≈9 Wh/day**. Size the panel for the worst month, not the best.

**USB unplugged:** the FT232RL is powered from USB VBUS and goes inactive. Fit **1 kΩ series resistors** on TXD/RXD and a **10 kΩ pull-up on PD0 (RXD)** so the unpowered module cannot load the MCU or leave the UART floating. Radio, flash, and logging stay on the pack.

**Not implemented:** battery voltage is not divided down or reported; `CMD_STATUS` still returns only used/free record counts (H10). Do not claim battery telemetry until an ADC channel and status field exist.

## Parts on this board

| Function  | Part               | EasyEDA          | Mount                 |
| :-------- | :----------------- | :--------------- | :-------------------- |
| MCU       | ATmega328P-PU      | `ATMEGA328P-PU`  | DIP-28 socket         |
| Radio     | **Ra-02**          | `Ra-02`          | Female 2.54 mm        |
| Flash     | W25Q64             | `W25Q64`         | DIP-8 or SOIC adapter |
| USB       | FT232RL module     | `FT232RL Module` | Headers               |
| Battery   | 2-pin 5.08 mm terminal | `Terminal block 5.08` | THT edge        |
| Inlet protection | 2× SS34 + 1 A polyfuse | `SS34` | THT              |
| Regulator | LM1117-3.3 TO-220  | `LM1117-3.3`     | THT                   |
| Antenna   | SMA + outdoor whip | `SMA-TH`         | Edge                  |
| Clock     | 16 MHz + 2× 22 pF  | —                | THT                   |

**3.3 V rail:** MCU pin 7, Ra-02 VCC, W25Q64 VCC. **GND:** MCU 8 & 22, radio, flash, FTDI, SMA legs.

Must match `receiver.c`:

| MCU pin | Net | Goes to |
|:--------|:----|:--------|
| 19 / 17 / 18 | SCK / MOSI / MISO | Ra-02 and W25Q64 |
| 16 PB2 | `RADIO_CS` | Ra-02 NSS |
| 15 PB1 | `FLASH_CS` | W25Q64 CS |
| 4 PD2 | INT0 | Ra-02 DIO0 |
| 3 PD1 TXD | UART | FTDI RX |
| 2 PD0 RXD | UART | FTDI TX |
| 9 / 10 | XTAL | 16 MHz + 22 pF |
| 1 RESET | — | 10 kΩ to 3.3 V |

UART 115200 host setting; `receiver.c` currently generates 111.1 kbaud (H9). Host protocol: [[Firmware Hub#Data formats]].

## Recommended passives (RX)

| Part | Value | Where |
|:-----|:------|:------|
| Crystal | **16 MHz** + **2× 22 pF** | XTAL1 / XTAL2 |
| RESET pull-up | **10 kΩ** | RESET → 3.3 V |
| CS pull-ups | **100 kΩ** | RADIO_CS, FLASH_CS → 3.3 V |
| Regulator input | **10 µF** | USB 5 V **and** BATT, near LM1117 in |
| Regulator output | **10 µF + 0.1 µF** | 3.3 V near LM1117 out |
| UART series | **1 kΩ × 2** | TXD / RXD, so an unpowered FT232 cannot load the MCU |
| RXD pull-up | **10 kΩ** | PD0 → 3.3 V; keeps UART idle high when FT232 is unpowered |
| MCU decouple | **0.1 µF** | VCC and AVCC within 5 mm |
| Radio decouple | **0.1 µF + 10–47 µF** | at Ra-02 VCC (RX current quieter than TX PA, still good practice) |
| Flash decouple | **0.1 µF** | at W25Q64 VCC |

**Optional (nice for outdoor tower)**

| Part | Why |
|:-----|:----|
| Power LED + 1 kΩ on 3.3 V | Visual “alive” |
| Activity LED on DIO0 or software GPIO | Packet indication |
| Low-capacitance TVS at SMA | Antenna ESD / surge assist (**IEC 61000-4-5** thinking) |
| Polyfuse on USB 5 V | Cable fault protection |
| Extra bulk 100 µF on 5 V | Long USB cable sag |

**Behaviour**

| Mode | What happens |
|:-----|:-------------|
| PC unplugged, pack fitted | DIO0 → CRC-ok frame → 16-byte append (`record_id` + 12 B) on pack power |
| PC plugged | Same; dashboard **polls** `'S'`/`'D'`/`'N'`; USB also feeds the regulator through its diode |
| Host `'D'` / `'N'` | `0xAA` header + records (not a raw 12-byte stream) |

Do **not** put a FAT filesystem on the AVR. W25Q64 is a raw page log: [[Firmware Hub#Surface tower (`receiver.c`)]].

**~$15.50.** Antenna **1.5–2 m** up: [[Project Overview#Radio]].
