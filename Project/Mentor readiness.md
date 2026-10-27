---
tags:
  - slf-ewn
  - feasibility
  - standards
aliases:
  - Feasibility FAQ
  - Feasibility pack
  - Compliance readiness
parent: "[[Project Overview]]"
status: active
---

# Feasibility and standards notes

← [[SLF-EWN Hub]] · Decisions: [[Project Overview]] · IEC detail: [[Project Overview#IEC standards and feasibility]] · Gaps: [[Status and open issues]] · Challenges: [[Challenges and Solutions]]

Working notes on the questions that come up when the design is checked for feasibility: standards, radio rules, safety, cost, power, RF, ethics, and what is still unproven.

Last updated: 2026-10-25.

---

## 1. Overall feasibility

| Question | Answer |
|:---------|:-------|
| Is the architecture feasible? | **Yes.** Buried moisture + tilt, 433 MHz LoRa, primary cell, surface tower, and a Qt host is a known class of environmental IoT |
| Is it blocked by IEC? | **No.** See §2 and [[Project Overview#IEC standards and feasibility]] |
| Is it blocked by radio law? | **No in principle.** 433 MHz ISM/SRD is a standard student/industrial path; local duty-cycle and power limits still apply (§3) |
| Can it be built with the team constraints? | **Yes.** 1-layer THT, shop parts, AVR-GCC, Qt; the node + tower pair comes in under $75 |
| Is multi-year life proven? | **Designed, not measured.** Sleep current and seal tests are still needed before any burial claim |
| Is through-soil range proven? | **Designed from literature, not measured.** A soil-barrel or pit test is still required |
| Is this a certified life-safety system? | **No.** It is a research / early-warning prototype and decision-support tool, not IEC 61508 SIL equipment |

**Short statement for the report**

> SLF-EWN is a feasible engineering prototype: the sensing, radio, power, and host approach align with established IEC equipment domains and common sub-GHz IoT practice. Critical performance claims (µA sleep, soil RF, multi-year seal) remain subject to measurement; the system is not claimed as certified functional-safety equipment.

---

## 2. Standards (IEC / ISO)

Full tables: [[Project Overview#IEC standards and feasibility]] · [[Full Proposal#8. Standards and feasibility]].

| Domain | Reference | Feasible? |
|:-------|:----------|:---------:|
| Ingress (IP) | IEC 60529 | Yes, design toward IP67/68-class capsule |
| Environment tests | IEC 60068 | Yes, qualification path exists |
| Impact | IEC 62262 | Yes, tower boxes |
| EMC | IEC 61000-6-x, 61000-4-x | Yes, design toward; lab later if required |
| ICT safety | IEC 62368-1 | Yes, tower / USB / host |
| Primary cell | IEC 60086 | Yes, Li-SOCl₂ class |
| Landslide EWS process | ISO 22327 / 22328 | Yes, process guidance, not a PCB ban |
| Functional safety SIL | IEC 61508 | **Not claimed** for this prototype |

Referencing a standard is not the same as holding a test certificate.

---

## 3. Radio spectrum and EMC

| Topic | Position |
|:------|:---------|
| Band | **433 MHz** licence-exempt / ISM-style short-range device (region-dependent) |
| Typical EU technical refs | **ETSI EN 300 220** (SRD), **ETSI EN 301 489** (radio EMC) |
| Design mitigations | Short 12-byte frames, low duty cycle (~15 min), modest TX power class, matched SF/BW |
| National duty | Confirm **local** power and duty-cycle limits before field TX |
| EMC of the equipment | IEC 61000 family; PCB layout (short RF, decoupling, ADC separation) is the practical response |

Using 433 MHz LoRa for sparse telemetry is standard practice. Feasible does not mean type-approved.

---

## 4. Electrical and battery safety

| Topic | Position |
|:------|:---------|
| Buried node voltage | **3.6 V** primary Li-SOCl₂, low shock hazard |
| No 5 V ISP on buried board | Pre-flash in lab; avoids stressing a 3.6 V design with a 5 V programmer |
| Storage | 2-pin jumper isolates the cell |
| Tower | USB 5 V or 5.5–9 V pack → 3.3 V regulation; treated as ICT equipment (**IEC 62368-1**) |
| Lithium transport | Shipping cells may need **UN 38.3** rules (logistics, not design feasibility) |
| Supercap | 5.5 V rated parallel buffer for PA peaks, ordinary practice |

---

## 5. Environmental and mechanical feasibility

| Topic | Position |
|:------|:---------|
| Burial ~1 m | Matches both the sensing intent and the soil RF segment |
| Seal | Design intent IP67/68-class; **must** be verified (potting, gaskets, cable exits) |
| Capsule | Cylinder preferred (soil load, auger install); SolidWorks deliverables still open |
| Tower | Raised vertical whip 1.5–2 m AGL; outdoor box IP65+ intent |
| Climate | Li-SOCl₂ and industrial parts chosen for wide temperature; still site-specific |

---

## 6. Power and lifetime feasibility

| Claim | Basis | Status |
|:------|:------|:------:|
| Cell energy ~8.64 Wh | 3.6 V × ~2.4 Ah | Design calc |
| 7-year average ceiling ~39 µA | Calendar arithmetic | Design calc |
| Target &lt; 10 µA average | Margin for cold/leakage | **Unmeasured** on hardware |
| Duty cycle active share ~1.7 µA | 50 ms / 900 s at 30 mA | Design calc |
| PA peak | **47 mF / 5.5 V** + **1.2 kΩ** RC + P-MOS load switch | Designed ($\Delta V\approx127$ mV, $I_\mathrm{max}=3$ mA) |
| `SIM_FAST_WAKE` | Must be off in production | Open **F1** |

Lifetime is feasible on paper; burial claims wait on measured sleep current and seal integrity.

Detail: [[Challenges and Solutions]] sections E1–E5 · [[Status and open issues]] H3, F1.

---

## 7. Sensing and warning feasibility

| Topic | Position |
|:------|:---------|
| Moisture + tilt at depth | Established early indicators; **not** a full slope-stability FEM |
| Thresholds | Host defaults + per-node overrides; status `0x80` forces CRITICAL |
| ISO EWS role | Aligns with monitoring and warning service elements of **ISO 22327** |
| False alarms / missed events | Residual risk; ML filtering out of scope |
| After a landslide | ID stays continuous; map pin may need a survey update; no GNSS on the node |

Scientifically and operationally plausible as a prototype instrument network; not a substitute for geotechnical design of a slope.

---

## 8. Cost and schedule feasibility

| Item | Figure |
|:-----|:-----|
| Node BOM | ~$27 |
| Tower BOM | ~$15.50 |
| Pair | **&lt; $75** |
| Working timeline | ~12 days design→demo (aggressive; slips if fab or soil tests delay) |
| Team | A PCB/firmware · B mechanical · C dashboard · D film |

The cost is feasible for a student multi-node demo. Field campaigns and certification are extra budget.

---

## 9. Safety, ethics, and claims

| Principle | Our position |
|:----------|:-------------|
| Role of the system | **Decision support / research prototype.** It alerts operators; humans decide evacuation |
| Not claimed | Certified life-safety, SIL, or sole warning channel for a community |
| Data privacy | Air frames are currently cleartext; **encryption planned** (F7). IDs and moisture are sensitive at some sites |
| Responsible language | Prefer “early indicator” and “prototype EWS” over “guarantees lives saved” |
| Dual use | Slope monitoring for civil protection; the same technology can serve mining safety |

Can people rely on this tomorrow? Not as a sole certified system. As a validated prototype after measurement, yes for research and pilot support.

---

## 10. Technical risk register

| Risk | Severity | Mitigation / status |
|:-----|:---------|:--------------------|
| Sleep current too high | High | Measure H3; clear F1 |
| Soil RF fails link | High | Barrel/pit test H4; antenna air pocket |
| Moisture polarity wrong | Medium | F3 invert/calibrate |
| Multi-node collisions | Medium | F2 jitter implemented (`15 min + id % 30 s`); CRC drops residual overlaps |
| Seal fails underground | High | Mechanical + IP-oriented potting |
| PCB never fabbed | High | H1 |
| Over-claiming safety | Medium | §9 language discipline |

Live list: [[Status and open issues]].

---

## 11. Academic / engineering contribution

Not “the first landslide sensor in history.” What this build actually adds:

- Subsurface + through-soil **433 MHz** path with a fixed **12-byte** protocol
- **Pre-flash / no ICSP** on a buried 3.6 V board
- **P-MOS** gating + motion-INT dual-wake architecture
- Tower **NOR** log without FAT/MicroSD
- End-to-end **Proteus fake-SPI** path sharing the application hex
- Qt host with **Places / per-node thresholds**

Challenges narrative: [[Challenges and Solutions]].

---

## 12. Questions we expect

**Is this IEC compliant?**
It is aligned and feasible under the listed IEC families. It is not lab-certified in this phase.

**Is 433 MHz legal here?**
It is a standard SRD/ISM-class choice. Local power and duty-cycle rules still have to be observed before permanent field TX.

**Will it last seven years?**
It is designed for that class of life. It has to be proven with a µA measurement and seal validation first.

**Will it work through one metre of soil?**
The design target is literature-supported. Prove it with a soil-barrel or pit test.

**Can this replace a geotechnical consultant?**
No. It is an instrument and warning aid.

**Why not Wi-Fi / 2.4 GHz / BLE?**
Soil attenuation and the energy budget — see challenges section C.

**Why not GPS on every node?**
Power, cost, and burial. ID + host map is the prototype split.

**Arduino or third-party sensor libraries?**
No. Drivers (ADXL345, SX1278, W25Q64, protocol) are self-implemented in `Code/`. Only AVR-GCC / avr-libc standard headers are imported — see [[Code Hub#Self-implemented firmware (supply-chain / trust policy)]].

**What is Phase 2 vs Phase 3?**
Phase 2 = field tower (solar, siren, telemetry, secondary LoRa, handheld, host registry). Phase 3 = advanced sensors/mesh/ML. Steps: [[Phase 2 and Phase 3 plan]].

**One-sentence summary?**
Feasible prototype under IEC equipment practice and common sub-GHz IoT methods; performance claims pending measurement; not a certified sole life-safety system.

---

## 13. Where the detail is

| Topic | Open |
|:------|:-----|
| Decisions / BOM | [[Project Overview]] |
| Formal write-up | [[Full Proposal]] |
| IEC tables | [[Project Overview#IEC standards and feasibility]] |
| Open bugs / tests | [[Status and open issues]] |
| Deep technical challenges | [[Challenges and Solutions]] |
| Plain-language slide notes | [[Challenges Presentation Script]] |
| Protocol bytes | [[Firmware Hub]] |
| This note | **This page** |
