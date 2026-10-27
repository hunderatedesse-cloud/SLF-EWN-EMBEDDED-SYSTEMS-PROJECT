---
tags:
  - slf-ewn
  - project
  - proposal
aliases:
  - Project Proposal
  - Design Changelog
  - Development Notes
parent: "[[SLF-EWN Hub]]"
---

# Project overview

← [[SLF-EWN Hub]] · Submission package: [[Submission Checklist]] · Formal text: [[Full Proposal]] · Design challenges: [[Challenges and Solutions]] · Live gaps: [[Status and open issues]] · Standards: [[Project Overview#IEC standards and feasibility]] · Feasibility notes: [[Mentor readiness]]

Buried nodes at **1 m**, **ADXL345** + capacitive moisture, **Ra-02** 433 MHz through soil, tower **W25Q64** + USB, **Qt 6 / QML**. Life target **3–7 years**. Five or more nodes per tower.

---

## What we decided

When notes disagree, **this table wins.** Older drafts that said LIS3DH, MicroSD, AO3401, or 2-layer SMD were removed from the vault — ignore them.

| Topic | Not this | **This (as built)** |
|:------|:---------|:--------------------|
| Accel | LIS3DH (`WHO_AM_I` 0x33, ~2 µA) | **ADXL345** (`DEVID` 0xE5; always-on rail for INT wake) |
| Radio | “RFM95W” as the shop part | **Ra-02** (SX1278). Same SPI map as RFM95W in Proteus |
| Tower log | MicroSD / filesystem | **W25Q64** SPI NOR (no FAT) |
| FET | AO3401 SMD | **ZVP2106A** TO-92 |
| PCB | 2-layer SMD | **1-layer THT** + jumpers |
| UI | Qt Widgets / web | **Qt 6 + QML** (practical, not a marketing mock) |
| Tower power | Dual Li-SOCl₂ only | **USB 5 V** or **rechargeable pack** on a 2-pin header (solar-charged) |
| CAD | Blender as the enclosure | **SolidWorks** print · Blender film |
| Firmware | Arduino core / one `.ino` / third-party device libs | **AVR-GCC C**, self-written drivers; only toolchain `.h` (avr/*, stdint, string) — [[Code Hub#Self-implemented firmware (supply-chain / trust policy)]] |

Shop stocks **ADXL345**; code and fakes target `DEVID 0xE5`. LIS3DH (~2 µA) remains a future option if available, but it is a different LGA footprint as well as a driver change — it is not a pin-compatible swap.

---

## Why this stack

Surface rain gauges and cameras see failure late. Subsurface moisture and deformation are earlier, but burial means power, waterproofing, soil load, and through-earth radio. Commercial systems are costly or need maintenance.

**Contributions this build actually makes:**

- Pre-flash on an ISP jig — **no ICSP** on the buried board (no 5 V programmer on a 3.6 V cell).
- **99.9%** hibernation: FET-gates **moisture + LoRa only**; ADXL345 stays on always-on rail for INT; WDT-ticked 15 min interval.
- Tower **W25Q64** log so the PC can be unplugged.
- Qt 6 / QML that **polls** the tower UART (`0xAA`) **or** runs simulation (never mixed).
- Proteus fake SPI slaves so the node loop and frame builder run in simulation; the `0x8D` radio path is sim-only (F5).

---

## Team and constraints

| Team | Owns |
|:-----|:-----|
| **A** | 1-layer PCB, Proteus, AVR firmware |
| **B** | [[Mechanical Hub\|Capsule / SolidWorks]] |
| **C** | [[Dashboard Hub\|Dashboard]] |
| **D** | Film (Blender) — dimensions from Mechanical Hub only |

**Must follow for this prototype**

1. **Dashboard:** practical engineering UI (SAFE / WARNING / CRITICAL). Qt 6 + QML. Match [[Firmware Hub#Data formats]] — no second protocol.  
2. **PCB:** one copper layer; crossings = jumper or top-side solder blob. 100% THT. Socket the MCU; female headers for Ra-02.  
3. **Firmware:** bare-metal AVR-GCC (no Arduino). One source tree for the sim and the socket; the LoRa driver uses the fake `0x8D` path only in simulation (F5). Change pins/bytes only in [[Firmware Hub]] / `Code/`, then the matching sim or PCB note.

---

## How a day in the field looks

**Node (as in `transmitter.c`).** Hibernate (WDT 8 s ticks; **`SIM_FAST_WAKE` → 1 tick in the sim build only**) **or** INT0 → sample → 12-byte CRC frame → LoRa TX → sleep. Production path uses **SX1278 registers** (not Proteus `0x8D`). Stagger: **15 min + (id % 30) s**. Default ID **`0x0104`**. 

**Tower (`receiver.c`).** DIO0 ISR clocks 12 bytes (Mode 0 for the real SX1278; the Proteus fake is Mode 1) → CRC → 16-byte flash record. The dashboard **polls** at 115200 (`0xAA` replies); the firmware currently generates 111.1 kbaud (H9). Unplugged PC: still logs to the 8 MB W25Q64 (~524,288 records); when full, a chip erase restarts the log.

Bytes: [[Firmware Hub#Data formats]]. Pins: [[Transmitter Simulation]] / [[Receiver Simulation]]. Sources: [[Code Hub]]. Flow: [[SLF-EWN Hub#Data flow (as built)]].

---

## Timeline (working days)

| Phase | Days | Output |
|:------|:-----|:-------|
| Design | 1–3 | Schematic, PCB, Proteus |
| Buy / fab | 4–5 | JLCPCB, parts |
| Firmware | 3–6 | Sleep, LoRa, ADXL345, flash |
| Dashboard | 5–7 | Qt, serial, SQLite |
| Assemble | 7–8 | Solder, pre-flash, pot |
| Test | 9–10 | Bench, soil barrel, µA |
| Docs | 10–12 | Report, demo |

---

## Bill of materials

**Pair < $75.** Node ~$27, tower ~$15.50. Board photos: [[Transmitter PCB]] · [[Receiver PCB]].

### Buried node (~$27.45)

| Part | Qty | Est. USD |
|:-----|:---:|--------:|
| ATmega328P-PU + DIP-28 socket | 1+1 | 2.80 |
| Ra-02 433 MHz | 1 | 5.00 |
| Capacitive moisture v1.2 | 1 | 3.50 |
| ADXL345 module | 1 | 2.50 |
| ZVP2106A TO-92 | 1 | 0.25 |
| Li-SOCl₂ AA ~2400 mAh | 1 | 4.00 |
| Supercap 47 mF / 5.5 V | 1 | 1.20 |
| Series R 1.2 kΩ (charge limit) | 1 | 0.05 |
| FR-4 1-layer 50×50 mm | 1 | 2.00 |
| PVC ~75–80 mm ID × 120 mm + caps | 1 | 3.00 |
| SMA-TH + female 2.54 mm headers | — | 1.40 |
| Passives (~15) | — | 1.50 |
| 2-pin jumper + 17 cm helix wire | — | 0.25 |

The 2-pin jumper is the master disconnect; the helix is ~17 cm of solid copper.

### Surface tower (~$15.50)

| Part | Qty | Est. USD |
|:-----|:---:|--------:|
| ATmega328P-PU + socket | 1+1 | 2.80 |
| Ra-02 | 1 | 5.00 |
| W25Q64 8 MB | 1 | 1.20 |
| FT232RL module | 1 | 3.00 |
| LM1117-3.3 TO-220 | 1 | 0.50 |
| FR-4 1-layer 95×70 mm | 1 | 2.00 |
| Passives + 16 MHz + 22 pF | — | 1.00 |

Shared jig: Arduino as ISP ~$25 (one for the lab). Optional outdoor 433 MHz whip for the tower.

---

## Power (node)

**Li-SOCl₂ 3.6 V (AA ~2400 mAh typical):**

- Energy density up to **~710 Wh/kg** — small capsule.
- Self-discharge **<1%/year** at 20 °C — required for 3–7 years.
- **−60…+85 °C** — buried temperature swings.
- **Flat** voltage until end of life — simpler brown-out story than Li-ion sag.

TX peaks **~120 mA** for ~50 ms (SF7 class ToA). A thionyl cell cannot source that peak cleanly, so the **transmitter** uses an RC buffer: **1.2 kΩ series** from the cell, then **47 mF / 5.5 V** on the always-on rail. Math (same as challenges **E5**): $\Delta Q = 0.006\,\mathrm{C}$, $\Delta V \approx 127\,\mathrm{mV}$, $I_\mathrm{max} = 3.0\,\mathrm{mA}$, $\tau = 56.4\,\mathrm{s}$, $5\tau \approx 4.7\,\mathrm{min} \ll 15\,\mathrm{min}$. Do **not** use a 2.7 V or 3.6 V memory-backup cap on a 3.6 V cell. The **receiver does not use this network** (USB or battery via the 2-pin header). See [[EasyEDA Hub#Circuit decisions (TX vs RX) — final]] · [[Challenges and Solutions#E5 — Optimized RC Network for the TX Current Pulse]].

**2-pin jumper (master disconnect):** storage and shipping (zero drain), safe handling while inserting the socketed MCU, no slide switch to fill with dirt.

BOD: the Arduino factory 2.7 V setting (`0xFD`) will **lock** the chip as the cell ages. Buried fuses: [[Firmware Hub#Pre-flash (off the PCB)]].

**Tower:** two inputs are diode-OR-ed into the LM1117 — **USB 5 V** and a **2-pin battery header** (5.08 mm terminal, **5.5–9.0 V**), each through an SS34. The higher source wins, so the pack carries the receiver when USB is unplugged and USB takes over when the pack is absent or flat. A fuse (1 A polyfuse) and the series Schottky give fault and reverse-polarity protection. Both a **6 V SLA** pack and a **2S LiFePO4 / Li-ion** pack with BMS sit inside the window; the board does **not** charge or protect the pack.

Charging is external: **solar panel → 6/12 V PWM charge controller → pack**, with the controller's load output or the pack BMS providing low-voltage disconnect. Budget in battery mode is **≈25 mA at 3.3 V** (Ra-02 RX ≈11 mA, MCU ≈9 mA, LM1117 quiescent ≈5 mA), or **≈30 mA from a 6 V pack → ≈4.3 Wh/day**. A 6 V 4.5 Ah SLA (27 Wh) gives **≈6 days without sun**; a 6 V 5 W panel at ~3 peak-sun hours and 60 % derating gives **≈9 Wh/day**. The FT232RL stays on USB VBUS, so it is inactive in the field; 1 kΩ series resistors on TXD/RXD stop an unpowered module loading the MCU UART. USB remains the dump/debug path. Battery voltage is **not** sensed in firmware (H10). Simulation may omit the regulator (5 V parts). Real Ra-02 on the PCB needs 3.3 V.

---

## Radio

Open-air LoRa can be **10–15 km**. Soil is a different channel.

| Effect | What happens |
|:-------|:-------------|
| Wet clay | ~**10–40 dB/m** attenuation (moisture dependent) |
| First metre | ~**10–40 dB** gone before the wave reaches air |
| Soil–air boundary | Wave **refracts** and spreads along the surface |
| After breakout | Ordinary LoRa in air — hundreds of metres |

Design target: **1 m through soil**, **100–200 m** once in air.

Airtime (explicit header, CRC on, preamble 8, BW 125 kHz, CR 4/5, 12-byte payload): **SF7 ≈ 41 ms**, SF8 ≈ 72 ms, SF9 ≈ 144 ms (standard LoRa symbol math). Design energy uses **~50 ms** class including sensor overhead. SF7 is code baseline; SF8/SF9 are margin if soil loss is worse. Sensitivity improves with SF; node TX is **+17 dBm configured** (`REG_PA_CONFIG 0x8F`, PA_BOOST; module capable of up to +20 dBm).

**λ/4 in air** (VF ≈ 0.95 for copper):

$$\lambda/4 = \frac{3\times10^8}{433\times10^6\times 4}\times 0.95 = \mathbf{16.45\ \mathrm{cm}}$$

In a material: $\lambda_{\mathrm{med}} = \lambda_{\mathrm{air}}/\sqrt{\varepsilon_r}$.

| Medium | $\varepsilon_r$ | Electrical length |
|:-------|:---------------:|:------------------|
| Air pocket | 1 | stays ~16.5 cm — **use this** |
| Wet clay | ~30 | shrinks ~80% → ~**3 cm**, and moisture **moves** $\varepsilon_r$ |

A bare wire in clay is never resonant for long. Wind **17.0 cm** of solid copper on a **5 mm** mandrel, **10–11 close turns at ~3.2 mm pitch** → **~3.4 cm** tall. Keep that coil in an **air-filled** plastic volume. Helix **vertical**; tower whip **vertical**. Tower antenna **1.5–2 m** on a pole (dirt at ground level eats the incoming wave). Buried capsule **level**. Tower: high-gain **omnidirectional outdoor** whip — the node helix is electrically small; the tower is not size-limited.

---


### Spreading factor (SF7–9) and ToA

| SF | Role |
|:---|:-----|
| **SF7** | **Baseline in code** — shortest time-on-air for 12-byte frames |
| SF8–9 | Design **margin** if soil loss is worse than expected |
| SF10+ | Avoid for routine reports (long ToA, collisions, energy) |

**ToA** ≈ **41 ms** pure airtime at SF7 (use **~50 ms** with sample overhead for \(\Delta Q\)). TX and RX SF/BW/CR must match.

### Link budget

$$P_{rx} = P_{tx} + G_{tx} + G_{rx} - L_{soil} - L_{air} - L_{misc}$$

Soil (\(L_{soil}\), ~**10–40 dB** for 1 m wet clay) is the main uncertainty (**H4**). After breakout, design air target is **100–200 m** with a tower whip **1.5–2 m AGL** (omnidirectional). PA is **PA_BOOST at +17 dBm configured** (`0x8F`; up to +20 dBm available); peak current is why the TX **47 mF + 1.2 kΩ** buffer exists. Detail: [[Firmware Hub#RF settings (production)]].

## Where we deploy

| Site | Why |
|:-----|:----|
| Mine walls / pit benches | Wall creep, water ingress; 7-year cell matches a mine phase; no cable for machines to snag |
| Highway / dam / abutment earthworks | Settlement and liquefaction during and after construction |
| Village hillsides | Dense cheap grid → evacuation lead time |

---

## Shop (Vector4Engineering)

![[Ra-02 vector4engineering.png]]
![[ADXL345 vector4engineering.png|ADXL345 — as-built accel]]
![[capacitive soil vector4engineering.png|Moisture v1.2]]

## Others
![[atmega328p.png]]
![[li-socl battery.png]]
![[Lora module.png]]
![[antena.png]]

Datasheets to keep: ATmega328P, ADXL345, SX1278/Ra-02, W25Q64, moisture v1.2.

---

## IEC standards and feasibility

**Feasibility check:** the project has to be **feasible** under normal IEC practice.  
**Conclusion:** yes. The concept (buried moisture + tilt node, 433 MHz LoRa, primary cell, surface tower, PC dashboard) sits inside established IEC domains. That means the approach is **allowed and designable** under those frameworks, not that we already hold accredited lab certificates.

### Feasibility statement

> SLF-EWN is feasible with respect to IEC practice: a sealed, battery-powered subsurface sensor with short-range sub-GHz radio and a surface acquisition unit falls within established IEC scopes for ingress protection (**IEC 60529**), environmental testing (**IEC 60068**), EMC (**IEC 61000**), ICT safety (**IEC 62368-1**), and primary batteries (**IEC 60086**). Radio operation is constrained by spectrum regulation (typically ETSI / national SRD rules), not by an IEC prohibition.

### IEC families mapped to this build

| Area | Standard family | Role for SLF-EWN |
|:-----|:----------------|:-----------------|
| Enclosure / water / dust | **IEC 60529** (IP code) | Capsule and tower housings; buried/wet intent → IP67/IP68-class design target; outdoor tower often IP65+ |
| Environmental robustness | **IEC 60068** series | Temperature, humidity, vibration, shock qualification path |
| Mechanical impact | **IEC 62262** (IK code) | Outdoor tower boxes |
| EMC (generic) | **IEC 61000-6-1** immunity, **IEC 61000-6-3** emissions | Residential / light-industrial baseline; industrial sites may use 6-2 / 6-4 |
| EMC (basic tests) | **IEC 61000-4-2** ESD, **-4-3** radiated RF, **-4-4** EFT, **-4-5** surge, **-4-6** conducted RF | Lab methods if formal EMC is required; layout already aims at quiet RF and decoupling |
| Electrical safety | **IEC 62368-1** | Strongest fit for **tower + USB + host**; outdoor ICT provisions; buried 3.6 V primary node is low electrical hazard |
| Primary batteries | **IEC 60086** series | Li-SOCl₂ long-life primary cells |

### Not IEC blockers

| Topic | Note |
|:------|:-----|
| 433 MHz LoRa | Spectrum use → **national rules + ETSI** (e.g. EN 300 220), not an IEC ban |
| Landslide EWS *process* | **ISO 22327:2018** / **ISO 22328** series (early-warning guidelines) — supports system role, not PCB design |
| Functional safety SIL | **IEC 61508** only if we formally claim a safety integrity level — **not** required for this prototype |
| Student prototype | IEC is written for products; we **reference** and design *toward* it; full compliance is a later industrial step |

### Mapping to our design choices

| Choice | Feasibility note |
|:-------|:-----------------|
| Sealed capsule ~1 m | **IEC 60529** + **IEC 60068** path |
| Li-SOCl₂, FET gating, µA sleep | Primary-cell practice under **IEC 60086** context |
| Short 12-byte 433 MHz frames | Equipment EMC under **IEC 61000**; air interface under spectrum rules |
| Tower + USB + Qt | **IEC 62368-1**; outdoor surge/ESD under **IEC 61000-4-x** |
| Host thresholds / zones | Aligns with EWS process (**ISO 22327**); not an electrical conflict |

### What feasible does *not* mean

| Claim | Status |
|:------|:------:|
| Concept aligns with IEC domains | **Yes** |
| We already hold IEC test reports | **No** |
| Multi-year life without measurement | **No** — still measure sleep current, seals, soil RF |
| SIL claim under IEC 61508 | **Not claimed** |

Formal product certification would need an accredited lab later. This prototype phase designs toward these standards and closes critical risks by measurement (see [[Status and open issues]]).

Also summarised in [[Full Proposal#8. Standards and feasibility]].

**Broader feasibility questions** (spectrum, safety claims, cost, power proof, ethics, FAQ): [[Mentor readiness]].

---

## Later (not this build)

Pads on the TX board for an **external digital extensometer** — not stuffed in this prototype.

**Draw-wire.** High-tensile steel from the capsule to stable rock or a deep stake. A rotary encoder measures pull. ~**0.1 mm** of creep is visible as counts on the AVR. Compact; SPI/I²C or quadrature depending on the module.

**TDR.** Cheap coax through the slope. A shear plane pinches the cable and changes impedance; a pulse locates the pinch to centimetres. Needs a fast analog front-end — good as **report future work**, not this BOM.

**In-place inclinometer.** SCA103T-class (or similar) logs slow angular creep over days. Qt charts a rising slope hours before acceleration. ADXL345 still covers **sudden** shocks.

**Out of scope for this prototype BOM (planned Phase 2):** see [[Data path future architecture]] —

- **Secondary LoRa on the tower** (node RX stays on primary Ra-02; backhaul does not block listening).  
- **USB on the tower kept** for dump, debug, firmware update.  
- **Tower siren:** critical node message (`0x80`) latches loud alarm; host/handheld **SILENCE** when linked; otherwise alarm continues.  
- **Tower field power:** panel, charge controller, and pack sizing (the RX board now carries the battery inlet header; enclosure integration and autonomy testing remain).  
- **Tower self-telemetry:** battery, uptime, flash fill, siren/radio health logged and shown on host beside node data.  
- **Host registry:** SQLite + dashboard register **towers** (location/data), **nodes**, and **node↔tower** links.  
- **Handheld** MCU + mini LCD: concise status; collect via LoRa or USB from tower; USB data-mule to host.  
- **Host LoRa** for direct tower link when in range; otherwise handheld → USB → Qt.  
- Mesh hops in valleys; ML false-alarm filtering; extensometer/TDR/IPI as above.

Tracked gaps: [[Status and open issues]]. **Roadmap:** [[Phase 2 and Phase 3 plan]].
