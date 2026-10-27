---
tags:
  - slf-ewn
  - project
  - submission
  - checklist
parent: "[[SLF-EWN Hub]]"
aliases:
  - Submission Index
  - Submission Package
status: active
---

# Submission checklist — INSA 2026

**Event:** INSA 2026 — 5th Round Summer Camp · **Group 1** · Embedded Department
**Project:** Subterranean Landslide and Flood Early-Warning Network (SLF-EWN)
**Package version:** 1.0 · **Date:** 25 October 2026

This page is the index for the submission package. It says what to send, what to read first, what has actually been verified, and what must be stated honestly if a reviewer asks.

← [[SLF-EWN Hub]] · Formal report: [[Full Proposal]] · Slides: [[Challenges and Solutions]] · Gaps: [[Status and open issues]]

---

## 1. Package contents

| Item | File / note | Status |
|:-----|:------------|:-------|
| Formal report | [[Full Proposal]] | Ready |
| Presentation deck | [[Challenges and Solutions]] | Ready |
| Speaker notes | [[Challenges Presentation Script]] | Ready |
| Firmware sources | [[Code Hub]] (`Code/`) | Ready |
| Wire formats | [[Firmware Hub]] | Ready |
| PCB design notes | [[EasyEDA Hub]] · [[Transmitter PCB]] · [[Receiver PCB]] | Schematics done; images an older revision (H7) |
| Simulation notes | [[Proteus Hub]] | Ready |
| Dashboard notes | [[Dashboard Hub]] | Ready |
| Mechanical notes | [[Mechanical Hub]] | Specs set; CAD/printables pending (M1–M3) |
| Decision record / BOM | [[Project Overview]] | Ready |
| Feasibility and standards | [[Mentor readiness]] · [[Project Overview#IEC standards and feasibility]] | Ready |
| Open issues | [[Status and open issues]] | Ready |

**Stale drafts are gone.** The old LIS3DH / MicroSD / two-cell / 2-layer notes were removed from the vault; do not re-import them from Git history.

---

## 2. Reading order

1. **[[Full Proposal]]** — the report: problem, architecture, design, verification, limitations.
2. **[[Challenges and Solutions]]** — the slide deck: why each constraint forced a design response.
3. **[[Project Overview#What we decided]]** — one-page decisions table when documents disagree.
4. **[[Firmware Hub]]** — byte layouts, status bits, RF settings, pre-flash.
5. **[[Status and open issues]]** — what is not finished; read this before claiming field readiness.
6. **[[Code Hub]]** — source map for anyone who wants to rebuild the `.hex`.

---

## 3. Pre-submission checks

### Content

- [ ] All range figures say **100–200 m** after breakout, not 200–500 m.
- [ ] All flash text says **8 MB W25Q64**, **16-byte records**, **524,288 records**, **no header**, **chip-erase wrap**.
- [ ] All accelerometer text says **ADXL345**, raw 16-bit counts, **3.9 mg/LSB**, **1 g ≈ 256**.
- [ ] All antenna text says **10–11 turns**, 5 mm mandrel, ~3.2 mm pitch, ~3.4 cm.
- [ ] Capsule text says **75–80 mm ID × ~120 mm**, not 50 mm ID.
- [ ] RF sensitivity is quoted as **~−123 dBm at SF7 / 125 kHz**; −148 dBm only as the SF12 / 10.4 kHz device maximum.
- [ ] TX power is **+17 dBm configured** (`0x8F`), not a vague “14–20 dBm”.
- [ ] Node fuses are **L `0xE2`, H `0xDA`, E `0xFF`** (BOD disabled), not the Arduino `0xFD` 2.7 V setting.
- [ ] Jitter is described as **implemented**, with `SIM_FAST_WAKE` as the simulation override.
- [ ] UART is described as **111.1 kbaud actual** (H9), not “115200” without qualification.
- [ ] Tower power is described as a **2-pin 5.5–9 V inlet**, external solar charger/LVD, and **no battery telemetry** (H10).
- [ ] No claim that the PCB is fabricated or that the system is field-proven.

### Links and files

- [ ] Every image embed resolves to a file in `Attachments/`.
- [ ] Every internal link points to an existing note or heading.
- [ ] No citation markers, stray triple quotes, emoji, or assistant-style phrasing.
- [ ] PDF export includes the report and the slide deck; code is attached as a folder or archive.

### Demo

- [ ] Node `.hex` and tower `.hex` built from the tagged source.
- [ ] Proteus TX/RX sheets open at the correct rails (3.6 V / 5 V).
- [ ] Flash logging works in the sim build (JEDEC bypass per F4), so `'D'` and `'N'` return records.
- [ ] Dashboard opens the correct COM port, not `SIMULATION`.
- [ ] One full dump (`'D'`) and one incremental dump (`'N'`) rehearsed.
- [ ] Motion interrupt (fake ADXL345 INT1) drives status `0x80`; moisture pot changes the reported value.
- [ ] Map, Places, alert acknowledgement, and CSV export shown once.
- [ ] A spare MCU is pre-flashed and labelled.

---

## 4. Claims to state carefully

| Topic | Say this | Do not say |
|:------|:---------|:-----------|
| Verification | “Validated in Proteus; hardware build pending” | “Tested in the field” |
| Range | “Design target 100–200 m after breakout; soil loss unmeasured” | “Kilometres through soil” |
| Sensitivity | “About −123 dBm at SF7 / 125 kHz” | “−148 dBm” without the SF12 / 10.4 kHz condition |
| Battery life | “3–7 year target; sleep current not yet measured” | “7-year life demonstrated” |
| Tower backup | “2-pin 5.5–9 V inlet on the board; pack and solar sizing not built or tested” | “Runs for days on solar” |
| Flash | “8 MB, 524,288 records, wraps by chip erase” | “Infinite log” or “ring buffer” |
| Tilt | “Raw ADXL345 counts; host converts with asin” | “Direct degrees from the node” |
| PCB | “Schematics and layout drafted; fab not ordered” | “Board built and tested” |
| Standards | “Designed toward IEC/ISO families” | “Certified” or “compliant” |

---

## 5. Known issues to disclose if asked

| ID | One-line answer |
|:---|:----------------|
| F1 | `SIM_FAST_WAKE` is still on; production sets the 15 min interval. |
| F3 | Moisture ADC polarity still needs inversion/calibration. |
| F4 | The Proteus flash is a 25AA1024 EEPROM stand-in; it needs a JEDEC bypass, and the real W25Q64 path is untested. |
| F6 | The log wraps with a full chip erase; an incremental ring buffer is Phase 2. |
| F8 | The ADXL345 always-on rail draws tens of µA; the <10 µA target needs duty-cycling or a different part. |
| F9 | The dashboard tilt map is still linear; the correct conversion is `asin(raw/256)`. |
| F10 | Status bit 0 overlaps the sequence LSB; the fix is a 6-bit sequence plus an accel-error bit at bit 6. |
| H1 | PCB fabrication has not been ordered. |
| H4 | Through-soil RF loss has not been measured. |
| H7 | The exported schematic/PCB images are an older revision (LIS3DH, dual cell, EEPROM). |
| H8 | The tower runs 16 MHz at 3.3 V, outside the ATmega328P datasheet condition. |
| H9 | The tower UART is 111.1 kbaud, not 115200. |
| H10 | The tower has a 2-pin 5.5–9 V battery inlet, but the pack, panel, controller, LVD, and battery telemetry are not settled. |
| M3 | The capsule CAD still needs the 75–80 mm ID update. |

Full register: [[Status and open issues]].

---

## 6. Final sign-off

| Check | Owner | Done |
|:------|:------|:----:|
| Report facts match `Code/` | A | [ ] |
| Slide numbers match report | A | [ ] |
| PCB images labelled with revision | A | [ ] |
| Dashboard demo on the submission laptop | C | [ ] |
| Capsule dimensions match the report | B | [ ] |
| Film/renders use those dimensions | D | [ ] |
| Backup copy on a second USB drive | A | [ ] |

---

> [!warning] Rule of truth
> `Code/` wins over prose. If a number here disagrees with a `.c` file, fix the document — not the code comment — and record the change on [[Status and open issues]].
