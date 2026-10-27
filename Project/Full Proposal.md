---
tags:
  - slf-ewn
  - proposal
  - formal
  - submission
parent: "[[Project Overview]]"
aliases:
  - Full Project Proposal
  - Technical Report
status: submission-draft
---

# Subterranean Landslide and Flood Early-Warning Network

**SLF-EWN — Phase 1 design, implementation, and validation report**

| Item | Detail |
|:-----|:-------|
| Programme | **INSA 2026 — 5th Round Summer Camp** |
| Group | **Group 1 · Embedded Department** |
| Document | Phase 1 technical report and submission package |
| Version | **1.0** |
| Date | **25 October 2026** |
| Status | Submission draft — simulation-validated, hardware pending |
| Baseline | `Code/` node + tower firmware · EasyEDA Rev 1.0 · Qt 6 dashboard |

**Working decisions and BOM:** [[Project Overview]] · **Live gaps:** [[Status and open issues]] · **Wire formats:** [[Firmware Hub]] · **Sources:** [[Code Hub]] · **Presentation:** [[Challenges and Solutions]] · **Submission index:** [[Submission Checklist]]

---

## Table of contents

- [[#Abstract|Abstract]]
- [[#1. Introduction|1. Introduction]]
- [[#2. System architecture|2. System architecture]]
- [[#3. Hardware design|3. Hardware design]]
- [[#4. Firmware and protocol|4. Firmware and protocol]]
- [[#5. Dashboard software|5. Dashboard software]]
- [[#6. Mechanical enclosure and visualisation|6. Mechanical enclosure and visualisation]]
- [[#7. Testing, results, and verification|7. Testing, results, and verification]]
- [[#8. Standards and feasibility|8. Standards and feasibility]]
- [[#9. Deployment environments|9. Deployment environments]]
- [[#10. Bill of materials and timeline|10. Bill of materials and timeline]]
- [[#11. Challenges and design responses|11. Challenges and design responses]]
- [[#12. Limitations and open issues|12. Limitations and open issues]]
- [[#13. Future work|13. Future work]]
- [[#14. Conclusion|14. Conclusion]]
- [[#References|References]]
- [[#Appendix A — Pin maps|Appendix A — Pin maps]]
- [[#Appendix B — Protocol constants|Appendix B — Protocol constants]]
- [[#Appendix C — Pre-flash and assembly|Appendix C — Pre-flash and assembly]]

### List of figures

| Figure | Description | Location |
|:------:|:------------|:---------|
| 1 | System block diagram | [[#2. System architecture]] |
| 2 | Tower data path | [[#2. System architecture]] |
| 3 | Buried-node power tree | [[#3. Hardware design]] |
| T1–T4 | Transmitter schematic, PCB, and 3D views (Rev 1.0) | [[Transmitter PCB]] |
| R1–R4 | Receiver schematic, PCB, and 3D views (Rev 1.0) | [[Receiver PCB]] |

### List of tables

| Table | Description |
|:-----:|:------------|
| 1 | Subsystem responsibilities |
| 2 | Node power rails |
| 3 | Tower hardware |
| 4 | Air frame — 12 bytes |
| 5 | Flash record — 16 bytes |
| 6 | Host commands |
| 7 | Verification matrix |
| 8 | Standards mapping |
| 9 | Deployment environments |
| 10 | Bill of materials summary |
| 11 | Open limitations |
| A1 | Node pin map |
| A2 | Tower pin map |
| B1 | Protocol constants |

### Nomenclature

| Symbol / term | Meaning |
|:--------------|:--------|
| $\lambda$ | Wavelength |
| $\varepsilon_r$ | Relative permittivity |
| $\tau$ | RC time constant, $R \times C$ |
| ADC | Analogue-to-digital converter |
| AGL | Above ground level |
| AVR | 8-bit Microchip microcontroller family |
| BOM | Bill of materials |
| CRC | Cyclic redundancy check |
| CSS | Chirp spread spectrum |
| DIO | SX1278 digital I/O interrupt pin |
| FET | Field-effect transistor |
| ICSP | In-circuit serial programming |
| ISM | Industrial, scientific, and medical radio band |
| ISR | Interrupt service routine |
| LoRa | Long-range chirp spread-spectrum modulation |
| MCU | Microcontroller unit |
| NOR | Flash memory using NOR cells |
| PCB | Printed circuit board |
| PDU | Protocol data unit |
| QML | Qt modelling language |
| SF | LoRa spreading factor |
| SPI | Serial peripheral interface |
| THT | Through-hole technology |
| ToA | Time on air |
| UART | Universal asynchronous receiver/transmitter |
| WDT | Watchdog timer |

---

## Abstract

Landslides and flash floods threaten rural communities, mining, and civil infrastructure, yet most conventional monitors sit on the surface and see failure only after collapse has begun [1], [2]. The **Subterranean Landslide and Flood Early-Warning Network (SLF-EWN)** places autonomous, battery-powered nodes at **one metre** depth with a design target of **three to seven years** of maintenance-free operation. Each node measures capacitive soil moisture and three-axis acceleration (**ADXL345**), then sends a fixed **12-byte** telemetry frame through soil on **433 MHz LoRa (Ra-02 / SX1278)** to a surface tower. The tower stores accepted frames in **W25Q64 SPI NOR** as **16-byte** records and serves a native **Qt 6 / QML** dashboard over USB-UART.

Power management uses watchdog hibernation, a through-hole P-channel MOSFET (**ZVP2106A**) that gates the moisture sensor and radio, and a **pre-flashed** ATmega328P so the buried board needs no ICSP header. The build covers 1-layer through-hole PCBs, bare-metal AVR-GCC firmware with self-written drivers, Proteus validation using fake SPI peripherals, a printable cylindrical capsule, and Blender deployment visuals.

The prototype is **simulation-validated**, not field-proven. This report separates what has been demonstrated in Proteus from what remains open: through-soil RF loss, sleep current, PCB fabrication, and several firmware/dashboard mismatches listed in Section 12.

**Keywords:** landslide early warning, subsurface sensing, LoRa, ultra-low-power embedded systems, soil moisture, ADXL345, ATmega328P, AVR-GCC, SPI NOR flash, Qt 6, QML.

---

## 1. Introduction

### 1.1 Problem

Landslides and related flooding claim lives and destroy infrastructure every year [1]. Mountain villages, open-pit mines, and large earthworks are exposed to saturation and ground movement that **surface** rain gauges, cameras, and satellite imagery see only indirectly and often too late [2]. Cabled inclinometers fail when machines or weather cut the run.

Subsurface moisture and internal deformation sit closer to the failure mechanism. Putting electronics a metre underground imposes a hard power budget, waterproofing, soil pressure and corrosion, and **radio through wet earth** [3]. Commercial systems are expensive or need frequent visits that a buried node cannot accept. Recent low-cost LoRa monitoring networks demonstrate the sensing and telemetry case, but most still place sensors at or near the surface [4]–[6].

### 1.2 Objectives

1. **Autonomous burial** — a 3–7 year design target without opening the capsule or changing the cell.
2. **Direct hazards** — moisture saturation and three-axis tilt/vibration as primary indicators.
3. **Through-earth radio** — about 1 m of soil, clay, or sand; no buried cable.
4. **Aggregation** — one tower collecting and storing five or more nodes in local non-volatile memory, even with the PC unplugged.
5. **Visualisation** — a standalone desktop map, live values, and alarms when thresholds or a hardware interrupt fire.
6. **Form factor** — a rugged waterproof cylinder that fits an auger hole.

### 1.3 Contributions of this build

- **Pre-flash manufacturing** — chip programmed on a ZIF jig; no ICSP on the buried board; no 5 V programmer on a 3.6 V cell.
- **Cylindrical stack** — inline layout in a round tube; uniform soil pressure instead of corner stress.
- **Smart sleep** — long watchdog hibernation, short measure window, ADXL345 activity interrupt for emergencies.
- **Tower log** — W25Q64 raw record log instead of a MicroSD filesystem on an 8-bit MCU.
- **Qt 6 / QML dashboard** — polls the tower UART, stores telemetry in SQLite, separates admin/guest access, and isolates simulation from the COM port.
- **Proteus framework** — extra ATmega328P chips emulate the ADXL345 and Ra-02; the node loop and frame builder are exercised before fabrication, with a fake `0x8D` radio path (F5).

### 1.4 Scope and report structure

Section 2 describes the architecture and data path. Sections 3–6 cover hardware, firmware, dashboard, and mechanical design. Section 7 gives the verification matrix and the power/storage budgets. Sections 8–10 cover standards, deployment, and cost. Sections 11–13 state the design responses, the limitations that remain, and the future work. Appendices carry pin maps, protocol constants, and the pre-flash procedure.

---

## 2. System architecture

### 2.1 Three subsystems

```text
 Buried node (1 m)                          Surface tower
 ┌─────────────────────┐     433 MHz LoRa    ┌──────────────────┐
 │ Moisture v1.2       │  through earth      │ ATmega328P 16 MHz│
 │ ADXL345             │ ──────────────────► │ Ra-02 RX         │
 │ ATmega328P 8 MHz RC │                     │ W25Q64 flash     │
 │ Ra-02 TX            │                     │ FT232RL USB      │
 │ Li-SOCl₂ + 47 mF RC │                     └────────┬─────────┘
 │ ZVP2106A gate       │                              │ USB-UART
 └─────────────────────┘                              ▼
                                              Qt 6 / QML dashboard
                                              map · telemetry · alerts · SQLite
```

*Figure 1 — System block diagram. The tower runs from USB or a rechargeable pack (solar-charged); the buried nodes run from primary cells and are independent apart from their ID-derived wake offset.*

A handheld or Android client that talks to the tower over a second radio is **future work only** (Section 13).

### 2.2 Subsystem responsibilities

| Function | Buried node | Surface tower |
|:---------|:------------|:--------------|
| Sensing | Capacitive moisture + ADXL345 | None |
| Processing | ATmega328P, 8 MHz RC | ATmega328P, 16 MHz crystal |
| Radio | Ra-02 TX, 433 MHz | Ra-02 RX, 433 MHz |
| Storage | None | W25Q64, 16-byte records |
| Power | Li-SOCl₂ + supercap, gated rails | USB 5 V or 5.5–9 V pack → 3.3 V |
| Host link | Optional 9600 debug UART | USB-UART, command/response |

*Table 1 — Subsystem responsibilities.*

### 2.3 Buried-node loop

1. **Hibernate** — clocks off, MOSFET open, radio unpowered; this is the majority of the duty cycle.
2. **Wake** — watchdog ticks every 8 s; production interval is **15 min**, or INT0 fires on motion.
3. **Sensor rail on** — gate low, 5 ms settle, then ADC and SPI.
4. **Acquire** — ADC1 moisture and ADXL345 axes; pack ID, status, and CRC-8.
5. **Transmit** — gate on again; SPI to the radio (Proteus: `0x8D` FIFO contract; field: SX1278 `RegFifo`).
6. **Power down** — gate off, ADXL345 back to activity-wait mode, return to hibernate.

### 2.4 Tower loop and data path

```text
Ra-02 RxDone ── DIO0/INT0 ──► 12-byte frame buffer ── CRC-8 ──► 16-byte record
                                                                   │
                                                                   ▼
                                                              W25Q64 log
                                                                   │
                                      PC poll 'S'/'D'/'N'/'C' ◄────┘
                                             │
                                             ▼
                                  0xAA + command + length + payload
```

*Figure 2 — Tower data path. The tower never pushes unsolicited data; the host polls one command at a time.*

The tower checks CRC before accepting a frame. A rejected frame is dropped; it does not enter the log. With the PC unplugged, logging continues to the full 8 MB capacity.

### 2.5 Collision handling

One radio demodulates one packet at a time. Nodes therefore use a deterministic offset:

$$T_i = 15\ \mathrm{min} + (i \bmod 30)\ \mathrm{s}$$

The offset is implemented in `transmitter.c` and rounded down to whole 8 s watchdog ticks, so the realised period is 896–928 s rather than exactly 900 s. `SIM_FAST_WAKE 1` collapses the whole interval to one tick (~8 s) for bench and Proteus work, so simulated collisions are expected; the CRC check is the backstop. Production must disable `SIM_FAST_WAKE` (issue F1).

---

## 3. Hardware design

### 3.1 Microcontroller

**ATmega328P** on the node, tower, and Proteus slaves [7]:

- Power-down plus watchdog: the MCU core is sub-µA in power-down, but the 8 s watchdog oscillator adds a few µA and dominates MCU sleep current.
- 1.8–5.5 V supply range, compatible with a 3.6 V Li-SOCl₂ cell.
- SPI for radio, accelerometer, and flash; 10-bit ADC for moisture; GPIO for the gate and interrupts.
- DIP-28 package: socketed, pre-flashed, field-replaceable.

The buried node runs from its **8 MHz internal RC** oscillator (no crystal). The tower uses a **16 MHz crystal**. Brown-out detection is disabled (`0xFF`) or set to 1.8 V (`0xFE`); the Arduino factory 2.7 V (`0xFD`) and 4.3 V (`0xFC`) settings would lock the chip as the cell ages.

> **Open issue H8.** The ATmega328P is rated for **10 MHz maximum at 2.7–5.5 V** and 20 MHz only at 4.5–5.5 V. The tower runs 16 MHz from a 3.3 V rail, which is outside the datasheet condition. The Proteus model runs at this clock, but the design should move to a 5 V MCU with level shifting, an 8 MHz clock, or an explicitly accepted overclock.

### 3.2 Sensors and gate

**Moisture — capacitive v1.2.** Resistive probes electrolyse and corrode. The capacitive blade feeds ADC1; its leads are soldered to edge pads and its supply is switched by the FET. The current firmware maps the ADC linearly, while the probe output falls as moisture rises, so the reading is inverted (issue F3).

**Motion — ADXL345 (as built)** [9]. `DEVID` register `0x00` returns `0xE5`. The device is configured for full-resolution ±2 g and the firmware reads 16-bit axes. At 3.9 mg/LSB, **1 g ≈ 256 counts** and ±2 g ≈ ±512 counts. INT1 drives the MCU INT0 pin for motion wake. Status `0x80` marks an emergency wake; `0x01` marks an accelerometer `DEVID` failure. The shop stocks the ADXL345, so it replaced the lower-current LIS3DH in the bill of materials.

**Gate — ZVP2106A** P-channel MOSFET in TO-92. Driving the gate **LOW** connects 3.6 V to `VCC_SW`; the 100 kΩ pull-up keeps the rail off during reset. `VCC_SW` feeds **moisture and the Ra-02 only**. The ADXL345 stays on the always-on rail so its activity interrupt can wake the MCU.

```text
Li-SOCl₂ (+) ── 2-pin jumper ── 1.2 kΩ ──┬── 47 mF supercap (+)
                                         ├── MCU VCC / AVCC
                                         ├── ADXL345 VS + VDD_IO
                                         └── ZVP2106A SOURCE
ZVP2106A DRAIN = VCC_SW ── moisture VCC
                       └── Ra-02 3.3 V
```

*Figure 3 — Buried-node power tree. The supercap sits after the series resistor and supplies the transmit pulse.*

### 3.3 Radio and antenna

**Ra-02** (SX1278) on 2.54 mm female headers [8]. The production drivers configure:

| Parameter | Value |
|:----------|:------|
| Frequency | **433.0 MHz** ISM |
| Spreading factor | **SF7** baseline |
| Bandwidth | **125 kHz** |
| Coding rate | **4/5** |
| Header | Explicit |
| Payload CRC | On |
| Sync word | `0x12` |
| Preamble | 8 symbols |
| PA | **PA_BOOST, +17 dBm** (`PA_CONFIG 0x8F`); module capable of up to +20 dBm |

The SX1278 datasheet headline sensitivity is **−148 dBm at SF12 with a narrow 10.4 kHz bandwidth**. At the **SF7 / 125 kHz** setting used here, the receive sensitivity is approximately **−123 dBm**. All link budgets in this report use the SF7 figure; quoting −148 dBm for this configuration would overstate the margin by about 25 dB.

**λ/4 in air** (velocity factor ≈ 0.95 for copper):

$$\lambda/4 = \frac{3\times10^8}{433\times10^6\times 4}\times 0.95 = \mathbf{16.45\ \mathrm{cm}}$$

Wet clay ($\varepsilon_r \approx 30$) collapses the electrical length toward ~3 cm and detunes a bare wire. The helix therefore stays in an **air pocket** inside the capsule: 17.0 cm of solid copper on a **5 mm mandrel**, **10–11 close turns at ~3.2 mm pitch**, about **3.4 cm** tall, axis vertical. The Ra-02 has a u.FL connector, so a short **u.FL/IPEX → SMA** pigtail reaches the board-edge SMA.

**Through-soil budget.** Wet clay attenuates roughly **10–40 dB/m** at 433 MHz. The first metre therefore costs about **10–40 dB** before the wave reaches air. After breakout the link is ordinary LoRa in air. The design target is **1 m through soil** and **100–200 m** once in air, with the tower whip **1.5–2 m above ground**. This is still a target, not a measurement (issue H4).

### 3.4 Node power

**3.6 V Li-SOCl₂ AA** (~2400 mAh): high energy density, <1 %/year self-discharge, flat discharge, −60…+85 °C range. The cell cannot source the transmit peak cleanly, so the node uses an RC buffer. The budget uses a **120 mA design peak**; the SX1278 datasheet figure at +17 dBm is about **87 mA** (120 mA applies at +20 dBm). The calculation is:

$$\Delta Q = 120\ \mathrm{mA}\times 50\ \mathrm{ms} = 6.0\ \mathrm{mC}$$

$$\Delta V = \frac{\Delta Q}{C} = \frac{6.0\ \mathrm{mC}}{47\ \mathrm{mF}} \approx 127\ \mathrm{mV}$$

$$\tau = R C = 1.2\ \mathrm{k\Omega}\times 47\ \mathrm{mF} = 56.4\ \mathrm{s},\qquad 5\tau \approx 4.7\ \mathrm{min}$$

The 1.2 kΩ series resistor limits **cell inrush to about 3.0 mA**; the supercap supplies the radio peak. This is a cell-protection limit, not the load current. A 2-pin jumper provides master disconnect for storage and safe MCU insertion; there is no slide switch to fill with mud.

The transmit charge alone averages **6.7 µA** at one frame per 15 min, before MCU sleep, sensor quiescent current, and the ADXL345 always-on rail. Reaching the **< 10 µA** system target is therefore not yet demonstrated (issues F8, H3).

| Rail | Loads | Behaviour |
|:-----|:------|:----------|
| Always-on 3.6 V | MCU, ADXL345 | Sleep and motion interrupt remain alive |
| `VCC_SW` (FET) | Moisture, Ra-02 | Off between samples |
| Supercap | Parallel to cell, after 1.2 kΩ | Supplies transmit pulse |

*Table 2 — Node power rails.*

### 3.5 Tower hardware

| Function | Part | Mount |
|:---------|:-----|:------|
| MCU | ATmega328P-PU | DIP-28 socket |
| Radio | Ra-02 | 2.54 mm female headers |
| Log | W25Q64 (8 MB) | DIP-8 or SOIC adapter |
| USB | FT232RL module | Headers |
| Battery inlet | 2-pin 5.08 mm + 1 A fuse + 2× SS34 | THT edge |
| 3.3 V | LM1117-3.3 | TO-220, THT |

*Table 3 — Tower hardware.*

Power enters through two diode-OR-ed paths into the LM1117: **USB 5 V** and a **2-pin battery inlet** (5.5–9.0 V, 1 A fuse, SS34). The higher source wins, so a 6 V SLA or a 2S LiFePO4 / Li-ion pack keeps the tower logging when USB is unplugged, and USB takes over when the pack is absent. Charging is external — solar panel → 6/12 V PWM controller → pack — with the pack BMS or the controller's load output providing low-voltage cut-off.

Battery-mode draw is **≈25 mA at 3.3 V** (Ra-02 RX ≈11 mA, MCU ≈9 mA, LM1117 quiescent ≈5 mA), or **≈30 mA / ≈4.3 Wh/day from a 6 V pack**. A 6 V 4.5 Ah SLA (27 Wh) gives **≈6 days without sun**; a 6 V 5 W panel at ~3 peak-sun hours and 60 % derating gives **≈9 Wh/day**. The FT232RL stays on USB VBUS and is inactive in the field; 1 kΩ series resistors on TXD/RXD stop the unpowered module loading the MCU. Battery voltage is **not** sensed in firmware (H10). The radio and flash share SPI: radio CS on **PB2**, flash CS on **PB1**, DIO0 to **INT0** (PD2).

### 3.6 PCB rules

Both boards are **1-layer FR-4, 100% through-hole**. The transmitter is **50 × 50 mm**; the receiver is **95 × 70 mm**. Decoupling capacitors sit within 5 mm of each supply pin, power traces are 30–40 mil, and signal traces about 10 mil. Layer crossings use a jumper or a top-side solder bridge, never a second layer. The MCU is socketed, modules sit on female headers, and the moisture probe leaves through edge pads.

> **Open issue H7.** The exported schematic and PCB images currently show an **LIS3DH** footprint, a receiver with **two Li-SOCl₂ holders**, a supercap, and an EEPROM footprint. The build text and `Code/` specify **ADXL345**, **USB + battery-header power**, and **W25Q64**. The images are an earlier revision and must be re-exported before fabrication. See [[EasyEDA Hub]] and [[Transmitter PCB]] / [[Receiver PCB]].

---

## 4. Firmware and protocol

### 4.1 Software trust policy

Node and tower firmware use **self-written** device drivers and protocol code. The only non-project includes are AVR-GCC / avr-libc headers such as `avr/io.h` and `stdint.h`. No Arduino core, vendor library pack, or third-party LoRa/accelerometer driver is linked. The reason is supply-chain integrity: an opaque library blob can drift, be mirrored incorrectly, or be altered deliberately, and a buried node cannot be re-flashed in the field. Full policy: [[Code Hub#Self-implemented firmware (supply-chain / trust policy)]].

### 4.2 Air frame — 12 bytes, big-endian

| Offset | Size | Field |
|:------:|:----:|:------|
| 0–1 | u16 | Device ID (`0x0104` default) |
| 2–3 | u16 | Moisture, 0–10000 (0.01 % units) |
| 4–5 | i16 | Tilt X, raw ADXL345 counts |
| 6–7 | i16 | Tilt Y, raw counts |
| 8–9 | i16 | Tilt Z, raw counts |
| 10 | u8 | Status: `0x80` emergency, `0x01` accel error, sequence in low bits |
| 11 | u8 | CRC-8/SMBUS over bytes 0–10 |

*Table 4 — Air frame.*

The XYZ fields are **raw accelerometer counts**, not degrees. The host must apply the ADXL345 scale and `asin` per axis. A linear “−1950…1950 → −180…180” mapping is not physically correct (issue F9).

### 4.3 Flash record — 16 bytes

| Offset | Size | Field |
|:------:|:----:|:------|
| 0–3 | u32 BE | Record ID (append counter) |
| 4–15 | 12 B | Copy of the air frame |

*Table 5 — Flash record.*

The W25Q64 is a raw NOR log [10]; there is **no stored header**. Record *n* starts at `n × 16`. Capacity is:

$$N_{\max} = \frac{8\ \mathrm{MB}}{16\ \mathrm{B}} = \mathbf{524{,}288\ records}$$

On boot, `record_count` is recovered by scanning for the first slot whose first byte is still `0xFF`. Append erases the 4 KB sector when the pointer enters it, page-programs one 16-byte record, and increments the count. When the chip is full, a **chip erase** (`0xC7`) restarts the log at record 0. `CLEAR` is a real chip erase, not a counter reset. The long chip-erase pause is a known limitation (F6); a true incremental ring buffer is future work.

### 4.4 Tower ↔ dashboard

The host sends one command at a time and waits for a complete `0xAA` reply.

| Command | ASCII | Payload | Action |
|:-------:|:-----:|:--------|:-------|
| `0x53` | `S` | none | Status: `record_count` and free records (8 B) |
| `0x44` | `D` | none | Full dump of all records |
| `0x4E` | `N` | 4-byte BE last record ID | Incremental dump |
| `0x43` | `C` | none | Chip erase, count returns to 0 |

*Table 6 — Host commands.*

Every reply is `0xAA | command | uint32be length | payload`.

> **Open issue H9.** `receiver.c` calls `uart_init(8)` with `U2X = 0` at 16 MHz, which gives **111.1 kbaud**, while the dashboard opens **115200**. That is about **3.6 % low**, outside the usual 2 % error budget; the simulated link tolerates it. The least-bad 16 MHz setting is `U2X = 1` with `UBRR = 16` (**117.6 kbaud, +2.1 %**, still marginal); a 14.7456 MHz crystal with `UBRR = 7` gives exactly 115200.

### 4.5 Pre-flash and fuses

There is no ICSP header on the buried board. The MCU is programmed in a ZIF socket with an Arduino-as-ISP jig and AVRDUDE.

| Fuse | Value | Meaning |
|:-----|:------|:--------|
| Low | `0xE2` | 8 MHz internal RC, no CKDIV8 |
| High | `0xDA` | No bootloader, no debugWire |
| Extended | `0xFF` | BOD disabled; `0xFE` = 1.8 V if a detector is required |

The tower uses a separate **16 MHz crystal** fuse recipe; writing the node fuse values to the tower would leave it on the internal RC clock (issue H2).

### 4.6 Timing

The nominal production interval is **900 s**. The watchdog wakes the MCU every 8 s and counts ticks; the ID offset adds 0–29 s, both quantised to 8 s ticks (896–928 s realised). A frame occupies about **41 ms** of airtime at SF7, or roughly **50 ms** including sensor and rail settling. `SIM_FAST_WAKE` reduces the interval to one tick for simulation and must be disabled before burial (F1).

---

## 5. Dashboard software

The dashboard is a native **Qt 6 / QML** desktop console [11]. It polls the tower over USB-UART and never mixes a live COM session with simulation mode. Telemetry, alerts, node profiles, places, and accounts are stored in SQLite.

| Function | Implementation |
|:---------|:---------------|
| Node view | Cards with moisture, tilt, status, sparklines |
| Detail view | 24 h / 7 d / 30 d charts, acknowledge, zone label, per-node limits |
| Alerts | Warning/critical/hardware interrupt log, acknowledge, clear, CSV export |
| Map | OpenStreetMap or ArcGIS satellite, place polygons, click-to-place nodes |
| Access | Admin and read-only guest; PBKDF2-HMAC-SHA256 passwords; 30 s lock after five failures |
| Link | Port, baud, dump phase, RX statistics, simulation toggle |

Default thresholds are **38 % / 58 %** moisture and **8° / 16°** tilt magnitude. A status byte of `0x80` forces CRITICAL regardless of the analogue values.

The tilt conversion in the current UI still follows the old linear range. Because the node sends raw counts with 1 g ≈ 256, the correct conversion is:

$$\theta = \arcsin\!\left(\operatorname{clamp}\!\left(\frac{\mathrm{raw}}{256}, -1, 1\right)\right)\times\frac{180}{\pi}$$

This is tracked as issue F9. The dashboard also accepts ASCII debug lines and raw 12-byte frames when no command is in flight; `receiver.c` itself does not emit those ASCII lines.

---

## 6. Mechanical enclosure and visualisation

A **cylinder** (PVC or printed) spreads soil load, fits an auger hole, and matches the inline PCB stack. A 50 × 50 mm board has a **70.7 mm diagonal**, so a 50 mm inner diameter cannot fit it. The capsule specification is therefore:

| Spec | Value |
|:-----|:------|
| Inner diameter | **75–80 mm** |
| Length | **~120 mm** |
| Wall | 3–5 mm |
| Seals | IP68-class O-rings; epoxy at the probe exit |
| Antenna | Helix in an air pocket under the top cap |

The stack, top to soil, is: SMA and helix air pocket → Ra-02 on female headers → ATmega328P in DIP-28 → ADXL345, ZVP2106A, and ceramics → 47 mF supercap → Li-SOCl₂ AA → moisture blade through the bottom cap.

Team B owns the SolidWorks printables; Team D renders the film from those dimensions only and does not invent new millimetres. At the time of writing, the capsule CAD still needs to be re-modelled at 75–80 mm ID, and the printables and renders are not delivered (issues M1–M3).

---

## 7. Testing, results, and verification

### 7.1 What has been verified

| Test | Method | Result | Status |
|:-----|:-------|:-------|:-------|
| Node boot and frame build | Proteus TX sheet | `[NODE] ID=… TX-OK` | Verified in simulation |
| Accelerometer identity | Fake ADXL345, `DEVID` | `0xE5` accepted | Verified in simulation |
| End-to-end frame | Two COMPIM pairs | 12 bytes arrive, CRC accepted | Verified in simulation |
| CRC rejection | Corrupt frame | Frame dropped, no DIO0 | Verified in simulation |
| Flash append/dump | Proteus 25AA1024 stand-in | Append/dump once the JEDEC check is bypassed | Simulation only; W25Q64 path untested (F4) |
| Dashboard poll | Qt + COMPIM | `'S'` / `'D'` return `0xAA` headers | Verified in simulation |
| Real W25Q64 erase/wrap | Hardware | — | Not run |
| Through-soil RF | Soil barrel / pit | — | Not run |
| Sleep current | µA meter | — | Not run |
| Multi-node collisions | Two TX sheets | CRC fail on overlap | Expected with `SIM_FAST_WAKE`; production jitter is implemented |

*Table 7 — Verification matrix.*

The Proteus path is not the production radio path. The fake Ra-02 uses a thin `0x8D` FIFO contract, while the production `.hex` writes real SX1278 registers. Both carry the same 12-byte PDU, but only the production path proves the radio configuration.

### 7.2 Power budget

| Term | Value | Note |
|:-----|:------|:-----|
| Transmit current | ~120 mA | Design allowance; ~87 mA typical at +17 dBm |
| Transmit duration | ~50 ms | Airtime plus overhead |
| Charge per frame | 6.0 mC | $I \times t$ |
| Average transmit current at 15 min | **6.7 µA** | $6.0\ \mathrm{mC} / 900\ \mathrm{s}$ |
| Supercap droop | 127 mV | 47 mF |
| Recharge time constant | 56.4 s | 1.2 kΩ × 47 mF |
| 5τ | 4.7 min | Well inside 15 min |

The 6.7 µA transmit average leaves little room under the 10 µA target once the MCU, moisture probe, and ADXL345 always-on rail are added. The ADXL345 alone draws tens of µA in its always-on low-power mode, so the target probably requires duty-cycling the accelerometer or moving to a LIS3DH-class part (F8).

### 7.3 Storage budget

At 96 records per node per day (one frame per 15 min) and **10 nodes**, the tower accumulates 960 records/day:

$$\text{autonomy} = \frac{524{,}288}{960} \approx 546\ \mathrm{days}\ (\approx 1.5\ \mathrm{years})$$

After that the log wraps by chip erase. A single node at the same rate would fill the chip in about 15 years, so the tower capacity is sized for a small fleet rather than one node.

### 7.4 Bench and field test plan

1. **Pre-flash** — program the node and tower, read back fuses, confirm the node boots at 8 MHz and the tower at 16 MHz.
2. **Electrical** — check 3.3 V/3.6 V rails, gate polarity, supercap polarity, and current draw in each mode.
3. **Sensor** — dry/wet moisture calibration and a known-angle tilt check against the ADXL345 counts.
4. **Radio** — conducted test through an attenuator, then a soil barrel at 0.5 m and 1 m; record RSSI/SNR and packet error rate.
5. **Sleep** — measure average current with `SIM_FAST_WAKE` disabled over at least ten cycles.
6. **Multi-node** — run five IDs for 24 h and confirm the offset prevents repeated collisions.
7. **Dashboard** — full dump, incremental dump, clear, map placement, alert acknowledgement, and CSV export.

---

## 8. Standards and feasibility

**Feasibility statement.** The architecture is feasible under normal IEC practice: it does not conflict with the main families used for outdoor and environmental electronics. Feasible means the approach is *designable within those frameworks*, not that accredited certificates are already in hand.

A sealed, battery-powered subsurface node with short-range sub-GHz radio and a surface acquisition unit falls within established scopes for ingress protection (**IEC 60529**) [13], environmental testing (**IEC 60068**), EMC (**IEC 61000** series), ICT safety (**IEC 62368-1**), and primary batteries (**IEC 60086**). Operation on 433 MHz is governed by spectrum regulation (typically ETSI or national short-range device rules), not by an IEC prohibition. Landslide early-warning *process* guidance is **ISO 22327** / **ISO 22328** [12] (complementary, not IEC electrical).

| Domain | Standards | Application to SLF-EWN |
|:-------|:----------|:-----------------------|
| Enclosure | IEC 60529, IEC 62262 | Capsule and tower IP/impact intent |
| Environment | IEC 60068 | Climatic and mechanical qualification path |
| EMC | IEC 61000-6-x, IEC 61000-4-x | Emissions/immunity target; ESD, RF, surge |
| Safety | IEC 62368-1 | Tower, USB, host path |
| Primary cell | IEC 60086 | Li-SOCl₂ long-life primary |
| Warning process | ISO 22327, ISO 22328 | Community warning workflow context |

*Table 8 — Standards mapping.*

**Out of prototype scope unless formally claimed:** IEC 61508 functional-safety SIL. Full product compliance requires laboratory testing after the research prototype. This build designs *toward* the standards above and validates seals, sleep current, and RF path by measurement.

Full tables: [[Project Overview#IEC standards and feasibility]]. Feasibility notes, spectrum, safety language, cost, and FAQ: [[Mentor readiness]].

---

## 9. Deployment environments

| Site | Why it fits |
|:-----|:------------|
| Mine walls / pit benches | Wall creep and water ingress; a 7-year cell matches a mine phase |
| Highway, dam, and abutment earthworks | Settlement and liquefaction during and after construction |
| Village hillsides | Dense, low-cost grid buys evacuation lead time |

*Table 9 — Deployment environments.*

The common requirement is a slope where surface access is difficult, power is unavailable, and a dense grid matters more than per-node accuracy.

---

## 10. Bill of materials and timeline

| Assembly | Estimated cost |
|:---------|---------------:|
| Buried node | **$27.45** |
| Surface tower | **$15.50** |
| Node + tower pair | **$42.95** |
| Shared Arduino-as-ISP jig | ~$25 |
| Pair with shared jig | ~$68 |

*Table 10 — Bill of materials summary.*

Line items and part numbers: [[Project Overview#Bill of materials]]. The node cost is dominated by the Ra-02, Li-SOCl₂ cell, and moisture probe. The tower cost is dominated by the Ra-02, FT232RL module, and W25Q64.

The prototype plan is **twelve working days**: design, procurement, firmware, dashboard, assembly, test, and documentation. The team split is A (PCB, Proteus, firmware), B (SolidWorks), C (dashboard), and D (Blender).

---

## 11. Challenges and design responses

### 11.1 Flash wear

NOR cells program only 1→0; restoring a 1 requires a 4 KB sector erase. The log writes sequentially, so every sector carries the same share of wear, and a chip erase wraps the log when full. This avoids a hot header sector. A true incremental ring buffer would avoid the long erase pause and is Phase 2 work.

### 11.2 Through-soil RF

Wet clay attenuates roughly 10–40 dB/m at 433 MHz. The helix stays in an air pocket, the capsule stays level, and the tower whip is high and vertical. The link is still optimistic until soil-barrel measurements close issue H4.

### 11.3 Multi-year battery life

The Li-SOCl₂ cell cannot source the transmit peak alone, so the 47 mF + 1.2 kΩ network supplies it. Average current must stay well under 10 µA at the 15 min interval. Hibernation, FET gating, and real radio sleep are mandatory; the always-on ADXL345 is the main threat to the budget.

### 11.4 Frame size

Twelve bytes carry identity, moisture, three axes, status, and CRC without fragmentation. That keeps airtime near 41 ms at SF7, which bounds both energy and collision duration. A larger frame would force a higher spreading factor or a longer duty cycle.

### 11.5 Non-recoverable nodes

A buried node is effectively abandoned at depth. Mitigations are long cell life, a socketed pre-flashed MCU, and a dense grid that tolerates individual failures.

### 11.6 Parts availability

The moisture v1.2 probe, ZVP2106A, and Ra-02 are shelf-specific. Substitution would force schematic and Proteus re-validation.

The full quantitative treatment is in [[Challenges and Solutions]].

---

## 12. Limitations and open issues

The prototype is a working simulation-backed design, not a deployed instrument. The following limitations are known and tracked; IDs refer to [[Status and open issues]].

| ID | Limitation | Impact |
|:---|:-----------|:-------|
| F1 | `SIM_FAST_WAKE` still enabled | Wakes every ~8 s, not 15 min |
| F3 | Moisture ADC not inverted | Wet soil can read as low moisture |
| F4 | Proteus flash is a 25AA1024 EEPROM stand-in | Sim storage needs a JEDEC bypass; the W25Q64 path is untested |
| F6 | Flash wraps by full chip erase | Long pause at wrap; no incremental ring |
| F8 | ADXL345 always-on current | `< 10 µA` target unlikely without duty-cycling |
| F9 | Dashboard tilt mapping linear | Angles under-read; use `asin(raw/256)` |
| F10 | Status bit 0 overlaps sequence LSB | Accel-error frame can look like a normal frame |
| H1 | PCB fabrication not ordered | No hardware build yet |
| H2 | Tower fuse recipe not written | Tower clock configuration incomplete |
| H3 | Sleep current unmeasured | Battery life is still a calculation |
| H4 | Through-soil RF unproven | Range and link margin are estimates |
| H7 | Schematic/PCB images are an older revision | Images do not match the as-built text |
| H8 | 16 MHz at 3.3 V is out of spec | Tower MCU is overclocked |
| H9 | UART is 111.1 kbaud, not 115200 | Link is outside the 2 % error budget |
| M1–M3 | Printables/renders not delivered; capsule CAD not updated | Mechanical package incomplete |

*Table 11 — Open limitations.*

Two further points belong in the report even though they do not have their own issue IDs:

- **No encryption yet.** The air frame is authenticated only by CRC-8, which detects corruption but not tampering. Encryption is planned (F7).
- **Single tower.** The host link is a USB cable. The Rev 1 board now takes a rechargeable pack on a 2-pin inlet, but panel/controller sizing, enclosure integration, autonomy testing, and battery telemetry remain Phase 2 (Section 13).

---

## 13. Future work

### 13.1 Immediate engineering

- Disable `SIM_FAST_WAKE`, invert/calibrate the moisture ADC, and fix the UART divisor.
- Reconcile the schematic with the as-built ADXL345 / USB + battery-header / W25Q64 design, then re-export the board images.
- Re-model the capsule at 75–80 mm ID and deliver the STEP/STL set.
- Measure sleep current and through-soil loss before claiming field life or range.

### 13.2 Phase 2 architecture

Full write-up: [[Data path future architecture]].

| Element | Plan |
|:--------|:-----|
| Tower primary LoRa | Unchanged — continuous RX from buried nodes |
| Tower secondary LoRa | Backhaul to host or handheld, so node packets are not missed on one radio |
| Tower USB | Retained for dump and debug; field firmware update would need a bootloader, which is not fitted |
| Tower siren | Latched alarm on a critical frame (`status 0x80`); host or handheld can silence when a supervising link is active |
| Tower power | Pack on the Rev 1 2-pin inlet; panel, charge controller, low-voltage cut-off, and autonomy testing in Phase 2 |
| Tower telemetry | Battery, uptime, storage, siren, and radio health shown in a host tower panel |
| Host registry | Register towers, nodes, and node-to-tower assignment; map and link paths |
| Handheld | Embedded controller + small LCD; LoRa and/or USB to the tower; USB data-mule to the PC |
| Host | Qt advanced UI; LoRa when in range, otherwise import from the handheld |

Backhaul prefers incremental “records after ID *n*” and optional critical push; bulk history stays on USB. This is **Phase 2** after the USB prototype is validated.

### 13.3 Longer-term sensing

- Draw-wire extensometer, TDR, or IPI-class inclinometer (pads reserved on the transmitter).
- A LIS3DH-class accelerometer if lower active current is required — different footprint and driver, not a pin-compatible swap.
- Mesh hops in deep valleys and ML-based false-alarm filtering.
- Production ring-buffer wear levelling.

---

## 14. Conclusion

SLF-EWN is a complete path from a 1-layer buried through-hole node, through through-earth LoRa and a flash-backed autonomous tower, to a native desktop dashboard. The protocol is fixed at 12 bytes on air and 16 bytes in the log; the firmware is bare-metal AVR-GCC with self-written drivers. The node loop and frame layout are exercised in Proteus through the fake `0x8D` path, while the production driver writes real SX1278 registers.

The design is simulation-validated and costed at about **$43** for a node–tower pair. It is **not** yet field-proven: through-soil loss, sleep current, PCB fabrication, and several documented firmware/dashboard mismatches remain open. The architecture does not need a redesign to close them; it needs measurement, a schematic re-export, and the firmware corrections listed in Section 12.

---

## References

[1] United States Geological Survey, *Landslide Hazards Program*. https://www.usgs.gov/programs/landslide-hazards (accessed 25 Oct. 2026).

[2] D. Petley, “Global patterns of loss of life from landslides,” *Geology*, vol. 40, no. 10, pp. 927–930, 2012. doi:10.1130/G33217.1.

[3] I. F. Akyildiz and E. P. Stuntebeck, “Wireless underground sensor networks: Research challenges,” *Ad Hoc Networks*, vol. 4, no. 6, pp. 669–686, 2006. doi:10.1016/j.adhoc.2006.04.003.

[4] C. Wang, W. Guo, K. Yang, X. Wang, and Q. Meng, “Real-time monitoring system of landslide based on LoRa architecture,” *Frontiers in Earth Science*, vol. 10, art. 899509, 2022. doi:10.3389/feart.2022.899509.

[5] M. Chu, A. Patton, J. Roering, C. Siebert, J. Selker, C. Walter, and C. Udell, “SitkaNet: A low-cost, distributed sensor network for landslide monitoring and study,” *HardwareX*, vol. 9, art. e00191, 2021. doi:10.1016/j.ohx.2021.e00191.

[6] M. V. Ramesh, “Design, development, and deployment of a wireless sensor network for detection of landslides,” *Ad Hoc Networks*, vol. 13, pp. 2–18, 2014.

[7] Microchip Technology Inc., *ATmega328P Data Sheet*, DS40002061B, 2020.

[8] Semtech Corporation, *SX1276/77/78/79 Data Sheet*, Rev. 7, 2019.

[9] Analog Devices, *ADXL345: 3-Axis Digital Accelerometer Data Sheet*, Rev. E, 2013.

[10] Winbond Electronics, *W25Q64BV/JV 3V 64M-Bit Serial Flash Memory Data Sheet*.

[11] The Qt Group, *Qt 6 Documentation*. https://doc.qt.io/qt-6/ (accessed 25 Oct. 2026).

[12] International Organization for Standardization, *ISO 22327:2018 — Security and resilience: Emergency management — Guidelines for implementation of a community-based landslide early warning system*; *ISO 22328-1:2020 — General guidelines for the implementation of a community-based early warning system for disasters*.

[13] International Electrotechnical Commission, *IEC 60529:1989+AMD1:2013 — Degrees of protection provided by enclosures (IP Code)*.

**Local shelf (Vector4Engineering, Addis Ababa):** Ra-02, capacitive moisture v1.2, and ADXL345 in stock.

---

## Appendix A — Pin maps

### A.1 Buried node (`transmitter.c`)

| ATmega328P pin | Net | Destination |
|:---------------|:----|:------------|
| PB5 (pin 19) | SCK | Ra-02 SCK, ADXL345 SCLK |
| PB3 (pin 17) | MOSI | Ra-02 MOSI, ADXL345 SDI |
| PB4 (pin 18) | MISO | Ra-02 MISO, ADXL345 SDO |
| PB1 (pin 15) | `LORA_NSS` | Ra-02 NSS, 100 kΩ to 3.6 V |
| PB2 (pin 16) | `ACCEL_CS` | ADXL345 CS, 100 kΩ to 3.6 V |
| PD4 (pin 6) | `GATE` | ZVP2106A gate, 100 kΩ to 3.6 V |
| PD2 (pin 4) | INT0 | ADXL345 INT1 (activity) |
| PD3 (pin 5) | INT1 | Ra-02 DIO0 (TxDone) |
| PC1 (pin 24) | ADC1 | Moisture signal |
| PD1 (pin 3) | TXD | Optional 9600 debug header |
| PC6 (pin 1) | RESET | 10 kΩ to 3.6 V; no ICSP header |
| VCC / AVCC | 3.6 V | 0.1 µF each to GND |

*Table A1 — Node pin map.*

### A.2 Surface tower (`receiver.c`)

| ATmega328P pin | Net | Destination |
|:---------------|:----|:------------|
| PB5 / PB3 / PB4 | SPI | Ra-02 and W25Q64 |
| PB2 (pin 16) | `RADIO_CS` | Ra-02 NSS, 100 kΩ to 3.3 V |
| PB1 (pin 15) | `FLASH_CS` | W25Q64 /CS, 100 kΩ to 3.3 V |
| PD2 (pin 4) | INT0 | Ra-02 DIO0 (RxDone) |
| PD1 (pin 3) | TXD | FT232RL RXD |
| PD0 (pin 2) | RXD | FT232RL TXD |
| PB6 / PB7 | XTAL | 16 MHz + 22 pF each to GND |
| PC6 (pin 1) | RESET | 10 kΩ to 3.3 V |
| VCC / AVCC | 3.3 V | 0.1 µF each to GND |

*Table A2 — Tower pin map.*

---

## Appendix B — Protocol constants

```c
#define SLF_FRAME_LEN             12u
#define SLF_RECORD_SIZE           16u
#define SLF_LORA_CMD_FIFO_WRITE   0x8Du
#define SLF_STATUS_EMERGENCY      0x80u
#define SLF_STATUS_ACCEL_ERROR    0x01u
```

*Table B1 — Shared protocol constants from `Code/real/Common/slf_protocol.h`.*

| Item | Value |
|:-----|:------|
| CRC | CRC-8/SMBUS, polynomial `0x07`, init `0x00` |
| Byte order | Big-endian for multi-byte fields |
| Flash JEDEC ID | `EF 40 17` (W25Q64) |
| Flash commands | `0x06` WREN, `0x02` page program, `0x03` read, `0x05` status, `0x20` sector erase, `0xC7` chip erase |
| Radio frame | 12 bytes, fixed length, explicit header, CRC on |
| Host magic | `0xAA` |

---

## Appendix C — Pre-flash and assembly

### C.1 Node pre-flash

1. Insert a fresh ATmega328P into the ZIF socket of the Arduino-as-ISP jig.
2. Write the node fuses: low `0xE2`, high `0xDA`, extended `0xFF` (BOD disabled). Do not use the Arduino default `0xFD`; it enables a 2.7 V brown-out.
3. Flash `transmitter.hex`; verify by reading back the fuse bytes and the first flash page.
4. Label the MCU with its `DEVICE_ID`.
5. Remove the MCU and store it in conductive foam; do not insert it into a powered board.

### C.2 Assembly order

**Transmitter:** socket and passives → FET → headers, SMA, moisture pads, power jumper → pre-flashed MCU → cell and supercap → Ra-02, ADXL345, probe → power-on self-test → remove jumper for storage.

**Receiver:** socket, crystal, LM1117, and passives → headers, SMA, FT232RL, W25Q64 → MCU → USB. No supercap and no FET on the receiver.

### C.3 Power-on checks

- 3.6 V at MCU VCC and AVCC; 3.3 V at the tower rail.
- Gate high at reset; `VCC_SW` off until PD4 is driven low.
- ADXL345 `DEVID` = `0xE5`.
- Node debug line: `[NODE] ID=… TX-OK`.
- Tower reply begins with `0xAA` after `'S'`.

---

**Document control.** This report reflects the `Code/` baseline and the corrected facts in [[Firmware Hub]], [[EasyEDA Hub]], and [[Status and open issues]]. Where this document and the code disagree, the code wins. Update the code first, then this report.
