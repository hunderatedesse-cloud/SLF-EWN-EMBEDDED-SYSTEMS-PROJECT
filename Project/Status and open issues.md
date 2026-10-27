---
tags:
  - slf-ewn
  - status
  - open-issues
aliases:
  - Status
  - Open issues
  - TODO
parent: "[[SLF-EWN Hub]]"
status: active
---

# Status and open issues

← [[SLF-EWN Hub]] · Decisions: [[Project Overview#What we decided]] · Sources: [[Code Hub]] · Feasibility pack: [[Mentor readiness]] · Plan: [[Phase 2 and Phase 3 plan]]

Last reviewed: **2026-10-25**. When code lands a fix, tick the item here and update the matching hub — do not leave a second truth elsewhere.

---

## Overall phase

| Area | Status |
|:-----|:-------|
| Architecture / decisions | **Locked** — [[Project Overview#What we decided]] |
| AVR firmware (node + tower) | **Working** in Proteus; production path present in `Code/` |
| Protocol (12 B / 16 B / `0xAA`) | **Stable** — [[Firmware Hub#Data formats]] |
| Proteus TX / RX sheets | **Usable** end-to-end with COMPIM |
| EasyEDA TX / RX | Schematics done · layouts in review · **fab pending** |
| Qt dashboard | Functional (poll, map, Places, alerts) — polish ongoing |
| SolidWorks capsule | Specs set · **printables not delivered** |
| Blender film | Shot list set · **renders not delivered** |
| Field / soil-barrel test | **Not started** |
| IEC feasibility pack | **Documented** — [[Mentor readiness]] · [[Project Overview#IEC standards and feasibility]] |

---

## Open issues (firmware / protocol)

| ID | Issue | Severity | Notes |
|:---|:------|:---------|:------|
| F1 | **`SIM_FAST_WAKE` still `1`** | High before burial | Forces ~8 s wakes. `#undef` / remove for production — [[Firmware Hub#Watchdog (not 15 min in the sim build)]] |
| F2 | **TX time jitter** | Done | Implemented: `T_i = 15 min + (id % 30) s` → WDT ticks in `transmitter.c`. CRC still drops residual overlaps |
| F3 | **Moisture ADC not inverted** | Medium | Capacitive v1.2 / sim pot: wet → lower volts. Code maps linear; wet reads as **low %**. Invert or calibrate in firmware |
| F4 | **Flash path dual story** | Medium | Proteus uses a 25AA1024 EEPROM stand-in (ignores sector erase; JEDEC `EF 40 17` check must be bypassed in the sim build). Production `receiver_storage.c` targets W25Q64 with sector/chip erase. Keep sim and field behaviours documented separately |
| F5 | **Proteus LoRa is FIFO `0x8D` only** | Low (sim) | Field `.hex` uses real SX1278 registers in **SPI Mode 0** (`transmitter_lora.c` / `receiver_radio.c`). The fake RX also expects **Mode 1** clock-out. Do not ship the fake SPI contract or mode to hardware |
| F6 | **Tower flash wrap** | Low | Sequential log; chip erase when full. A true incremental ring buffer would avoid the long erase pause and is Phase 2 work |
| F7 | **Air-frame encryption (planned)** | Medium | Encrypt on node, decrypt on tower. Pre-shared keys at pre-flash; lightweight crypto for 8 MHz AVR; final frame size still to be decided — [[Challenges and Solutions]] |
| F8 | **ADXL345 low-power INT on hardware** | Medium | Motion INT → INT0 while moisture/LoRa FET-off. Verify activity threshold and accel sleep current. The ADXL345 always-on rail draws tens of µA, so the **&lt; 10 µA** system target probably needs duty-cycling or a LIS3DH-class part |
| F9 | **Dashboard tilt mapping** | Medium | Node sends **raw ADXL345 counts** (3.9 mg/LSB, 1 g ≈ 256), but the dashboard still documents a linear **−1950…1950 → −180…180** map. Use `asin(clamp(raw/256))` and clamp to ±90°, or angles will under-read |
| F10 | **Status bit 0 collides with sequence** | Medium | `status = seq_counter & 0x7F`; `0x01` is ORed for accel error, so an error frame is indistinguishable from a normal frame whose seq LSB is 1. A later revision should reduce the sequence to six bits (0–63) and put the accel-error flag on bit 6 |

---

## Open issues (hardware / fab)

| ID | Issue | Severity | Notes |
|:---|:------|:---------|:------|
| H1 | **PCB fab not ordered** | High | 1-layer THT TX 50×50 mm, RX 95×70 mm — [[EasyEDA Hub]] |
| H2 | **Pre-flash fuse recipe for tower** | Medium | Node fuses documented (`0xE2` / `0xDA` / `0xFF`, BOD off). Tower **16 MHz crystal** fuses are a separate recipe — write them down before first tower board |
| H3 | **Sleep current unmeasured** | High before burial | Target **&lt; 10 µA** with `SIM_FAST_WAKE` off. Needs µA meter on real hardware |
| H4 | **Through-soil RF unproven** | High | Design assumes 1 m soil + 100–200 m air. Needs soil-barrel / pit test |
| H5 | **Single-vendor parts risk** | Medium | Moisture v1.2, ZVP2106A, Ra-02 from local shelf — [[Project Overview#Shop (Vector4Engineering)]] |
| H6 | **Confirm EasyEDA: ADXL always-on; supercap TX-only** | Medium | Docs reviewed 2026-10-25. Before fab verify schematic nets — [[EasyEDA Hub#Circuit decisions (TX vs RX) — final]] |
| H7 | **Schematic/PCB images are an older revision** | High before fab | Exports show an **LIS3DH** footprint, receiver **dual cells**, **supercap**, and **EEPROM**; build text and `Code/` say **ADXL345**, **USB + battery-header power**, **W25Q64**. Re-export after reconciling, and label images with their revision |
| H8 | **16 MHz at 3.3 V is out of spec** | High | ATmega328P is rated **10 MHz max at 2.7–5.5 V** (20 MHz only at 4.5–5.5 V). The tower runs 16 MHz from a 3.3 V rail. Use 5 V MCU with level shifting, an 8 MHz clock, or accept a documented overclock |
| H9 | **UART baud is not actually 115200** | High | `receiver.c` calls `uart_init(8)` with `U2X=0` at 16 MHz → **111.1 kbaud** (about **3.6 % low**, outside the ~2 % budget). The least-bad 16 MHz setting is `U2X=1`/`UBRR=16` (117.6 kbaud, +2.1 %, still marginal); a 14.7456 MHz crystal with `UBRR=7` gives exactly 115200 |
| H10 | **Battery inlet added; charger, LVD, and telemetry still open** | Medium before field | RX now has a **2-pin 5.5–9 V inlet** (1 A fuse, SS34 OR into LM1117). Undecided: pack chemistry (6 V SLA vs 2S LiFePO4 / Li-ion + BMS), panel wattage, PWM controller model, and low-voltage cut-off. Firmware has **no battery ADC**; `CMD_STATUS` remains 8 bytes of used/free counts. Fit **1 kΩ** UART series resistors because the FT232 is USB-powered. Autonomy target not yet written or tested — [[Receiver PCB#Battery and solar input]] |

---

## Open issues (dashboard / mechanical)

| ID | Issue | Severity | Notes |
|:---|:------|:---------|:------|
| D1 | No separate Dashboard README | Low | Build notes live in [[Dashboard Hub]] only |
| M1 | SolidWorks deliverables unchecked | Medium | Capsule, tower box, STEP/STL, dimensioned drawing — [[Mechanical Hub]] |
| M2 | Blender shot list not rendered | Low | Film is demo/report only — dimensions must come from Mechanical Hub |
| M3 | **Capsule CAD not updated to 75–80 mm ID** | Medium | A 50×50 mm board has a **70.7 mm diagonal**; the old 50 mm ID drawing cannot fit it. Re-model the tube at **75–80 mm ID × ~120 mm** and re-check the helix air pocket |

---

## Phase plan

Fine-grained roadmap: [[Phase 2 and Phase 3 plan]] (Phase 1 exit gate → Phase 2 workstreams A–G → Phase 3 outline).

---

## Explicitly out of scope (this build)

Planned later (architecture frozen): [[Data path future architecture]]

- Tower **secondary** LoRa backhaul (primary stays node RX)
- Tower **siren / alarm** + host SILENCE command
- Tower **field power** sizing: panel, charge controller, enclosure integration, autonomy test (the 2-pin battery inlet is now on the RX board)
- Tower **self-telemetry** (battery/life/health) + host tower panel
- Host-side LoRa dongle / radio
- **Handheld** collector (LCD + LoRa + USB data mule to Qt)
- Multi-day tower autonomy run on pack + panel (no USB)

Still out of scope unless reopened:

- Mesh hops between buried nodes
- ML false-alarm filtering
- Draw-wire extensometer / TDR / IPI (pads reserved only)
- Android phone as the primary field UI


## How to close an item

1. Change the **`.c` / schematic / CAD** first.  
2. Update the **canonical hub** ([[Firmware Hub]], [[Code Hub]], [[EasyEDA Hub]], …).  
3. Tick or delete the row on this page.  
4. Do not “fix” prose in [[Full Proposal]] without the code/CAD change.
