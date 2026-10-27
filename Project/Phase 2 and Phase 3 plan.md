---
tags:
  - slf-ewn
  - planning
  - phase-2
  - phase-3
aliases:
  - Phase plan
  - Roadmap
parent: "[[Project Overview]]"
status: planned
---

# Phase 2 and Phase 3 plan

← [[Project Overview]] · Architecture freeze: [[Data path future architecture]] · Gaps: [[Status and open issues]] · Feasibility: [[Mentor readiness]]

**Purpose.** Fine-grained work list to follow when implementation starts. **Do not start Phase 2 hardware/firmware until Phase 1 exit criteria are met** (unless a named exception is agreed).

**Naming**

| Phase | Meaning |
|:------|:--------|
| **Phase 1** | Current prototype: buried node + single-radio tower (USB or battery inlet) + Qt over UART |
| **Phase 2** | Field-capable tower and multi-path data access (agreed architecture) |
| **Phase 3** | Advanced sensing, mesh, analytics, production hardening |

---

## Phase 1 exit gate (must close before Phase 2 field claims)

These are **not** Phase 2 features; they prove the core chain.

| ID | Work item | Done when |
|:---|:----------|:----------|
| P1-1 | **F1** Remove / `#undef SIM_FAST_WAKE` in field `transmitter.c` builds | Production hex uses real 15 min + stagger |
| P1-2 | **H3** Measure node sleep current (µA meter, FET off, ADXL low-power) | Number written in Status; target class &lt; 10 µA average with duty cycle |
| P1-3 | **H4** Soil-barrel or pit RF test (1 m class path) | Path-loss note + pass/fail vs design assumption |
| P1-4 | **H6** EasyEDA nets match [[EasyEDA Hub]] (ADXL always-on; supercap TX only) | Schematic review checklist signed |
| P1-5 | **H1** Fab + assemble TX and RX boards | Boards exist and POST |
| P1-6 | **F3** Moisture ADC polarity / calibration | Wet reads high % (or documented mapping) |
| P1-7 | **F8** ADXL `THRESH_ACT` tuned on hardware | Motion INT wakes MCU reliably |
| P1-8 | End-to-end: node → tower flash → USB `'N'`/`'D'` → Qt | Demo script works without Proteus |
| P1-9 | **H2** Tower fuse recipe written and applied | Crystal 16 MHz tower boots from flash |

**Phase 1 “good enough” demo:** one node, one tower, Qt SAFE/WARNING/CRITICAL, emergency `0x80` path, no Phase 2 radios required.

---

## Phase 2 — fine-grained plan

Architecture reference: [[Data path future architecture]].

### Workstream A — Tower local alarm (can start on current single-radio tower)

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| A1 | Choose siren (piezo vs 12 V panel siren) and driver (BJT/MOSFET) | BOM line + GPIO pin assignment |
| A2 | Schematic: GPIO → driver → siren; optional MUTE button | EasyEDA update on RX board or daughterboard |
| A3 | Firmware: on CRC-OK frame with `status & 0x80`, set `alarm_latched` | Siren output high while latched |
| A4 | USB command **SILENCE** (e.g. `'Q'`) clears latch | Document in [[Firmware Hub]] + [[Dashboard Hub]] |
| A5 | Qt: Acknowledge critical → send SILENCE when COM linked | Button + audit log line |
| A6 | Policy: new `0x80` re-arms after silence | Test procedure |
| A7 | Optional: local MUTE logs event to flash status row | Spec only if time |

**Exit A:** Critical node frame sounds siren; USB silence stops it; no host → stays on.

---

### Workstream B — Tower self-telemetry (battery / life / health)

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| B1 | Define status record format (typed flash row and/or live poll payload) | Struct: battery_mV, flags, uptime_s, record_count, siren, fw_id |
| B2 | Hardware: battery voltage divider (and solar sense if solar present) | ADC channel + calibration constants |
| B3 | Firmware: periodic status log + on-demand poll command | e.g. extend `'S'` or new `'T'` tower status |
| B4 | Qt **Tower panel**: voltage, uptime, flash fill, siren, last node heard | UI mock → implement |
| B5 | Handheld one-line summary (after Workstream E) | `BAT … SIREN … LOG %` |

**Exit B:** Host shows tower health beside node cards when linked.

---

### Workstream C — Tower field power (solar + battery)

**Rev 1 status:** the **2-pin 5.5–9 V inlet**, 1 A fuse, and SS34 OR into the LM1117 are on the board. The panel, controller, low-voltage cut-off, and autonomy test remain.

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| C1 | Load budget: continuous RX + MCU + secondary LoRa idle + siren peak | Spreadsheet (first pass ≈30 mA / ≈4.3 Wh/day) |
| C2 | Select panel W, battery chemistry (e.g. LiFePO₄ or SLA), charge controller | BOM |
| C3 | Power architecture: solar → controller → battery → 5.5–9 V inlet; USB bench path diode-OR (**board part done**) | Schematic block |
| C4 | Enclosure: outdoor box, antenna feed, ventilation/seal | Mechanical note |
| C5 | Autonomy target written (e.g. *N* days no sun) and tested | Test log |

**Exit C:** Tower runs 24 h without USB; USB still works for service.

---

### Workstream D — Secondary LoRa backhaul (tower ↔ host)

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| D1 | Second Ra-02 on tower: pins, CS, DIO, antenna (no share with node RX) | EasyEDA + pin map |
| D2 | Air protocol: request “records after ID *n*” + compact tower status | Spec mirroring USB `'N'` |
| D3 | Tower firmware: secondary radio TX/RX state machine; primary stays continuous RX | Code module |
| D4 | Host LoRa path: USB-LoRa dongle or MCU bridge → same record parse as UART | Qt link mode “LoRa” |
| D5 | Duty-cycle / batching: no full chip dump over air by default | Policy in Firmware Hub |
| D6 | Optional: push CRITICAL/`0x80` promptly on secondary radio | Event TX |
| D7 | Range test ~1 km class (open air); document margin | Field note |

**Exit D:** Host can pull new records from tower without USB when in radio range.

---

### Workstream E — Handheld collector

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| E1 | MCU + mini LCD + LoRa + USB device capability + local store | BOM + block diagram |
| E2 | UI screens: link state, last nodes, SAFE/WARN/CRIT counts, tower BAT line | Wireframes |
| E3 | LoRa poll tower (same backhaul protocol as host) | Firmware |
| E4 | USB to tower (optional alternate when standing at pole) | Same record intake |
| E5 | Local session store (flash) | Capacity target documented |
| E6 | USB gadget to PC: export session for Qt import | File or bulk transfer format |
| E7 | SILENCE command when handheld is supervisor | Ties to Workstream A |
| E8 | No map/Places on handheld (host only) | Scope lock |

**Exit E:** Staff collect data in field and import to Qt without permanent host↔tower radio.

---

### Workstream F — Host registry (towers, nodes, connections)

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| F1 | SQLite schema: `towers`, `nodes.tower_id`, host link metadata | Migration |
| F2 | Settings → **Towers** CRUD (label, lat/lon, identity) | UI |
| F3 | Settings → **Nodes**: assign home tower | UI |
| F4 | Map: tower markers + nodes; filter by tower | UI |
| F5 | Link session: select active tower path (USB / LoRa / handheld file) | UI |
| F6 | Import handheld session → samples keyed by node + tower | Pipeline |

**Exit F:** Multi-tower data model usable even if only one tower is deployed.

---

### Workstream G — Security and protocol (Phase 2 slice)

| Step | Task | Deliverable |
|:-----|:-----|:------------|
| G1 | **F7** Choose lightweight cipher + nonce plan for 12-byte class PDU | Design note |
| G2 | Pre-shared keys at pre-flash; tower decrypt | Code + key handling SOP |
| G3 | Keep self-implemented driver policy ([[Code Hub]]) | No third-party middleware regression |
| G4 | Re-budget ToA / flash if frame grows | Update Challenges B/I math |

**Exit G:** Optional encrypted air path demo (can lag D/E if needed).

---

### Phase 2 suggested order (dependencies)

```text
P1 exit gate
    │
    ├─► A Siren + USB silence          (fast win on current tower)
    ├─► B Telemetry schema + poll      (parallel with A)
    ├─► F Registry schema early        (avoid UI rewrite later)
    │
    ├─► C Solar + battery              (needed before unmanned field)
    ├─► D Secondary LoRa + host LoRa
    ├─► E Handheld
    └─► G Encryption                   (can trail radio path)
```

### Phase 2 definition of done

- [ ] Tower solar + battery operational; USB service path kept  
- [ ] Siren on `0x80`; host/handheld silence when linked  
- [ ] Tower battery/uptime/flash visible on host  
- [ ] Secondary LoRa backhaul **or** handheld mule path proven end-to-end  
- [ ] Host registers tower location, nodes, node↔tower links  
- [ ] Phase 1 measurement claims (sleep, soil RF) still held on production hex  

---

## Phase 3 — outline (after Phase 2)

Not scheduled in detail until Phase 2 mid-point. Fine grain later; themes:

| Theme | Candidate work |
|:------|:---------------|
| **Sensing** | Draw-wire extensometer, TDR, IPI-class inclinometer (pads reserved on TX) |
| **Radio network** | Mesh / relay in valleys; multi-hop backhaul |
| **Analytics** | ML / rules to cut false alarms; category templates bulk apply |
| **Storage** | Production ring-buffer wear-leveling on W25Q64 |
| **Parts** | LIS3DH-class part if lower accel current required (footprint + driver change) |
| **Compliance** | Move from IEC *feasibility* notes toward formal test evidence where required |
| **Ops** | Multi-site host, hardened outdoor BOM, spares/pre-flash factory flow |

**Phase 3 entry idea:** Phase 2 DoD checked; at least one site runs unmanned for an agreed soak period.

---

## Tracking

| Artifact | Role |
|:---------|:-----|
| This page | Master step list |
| [[Data path future architecture]] | What Phase 2 *is* (design freeze) |
| [[Status and open issues]] | Live blockers (P1 IDs + later Phase 2 tickets) |
| [[Mentor readiness]] | Feasibility and standards answers for review questions |

**Implementation rule:** When a Phase 2 step starts, open a row under Status (e.g. `P2-A3`) and close it when the deliverable exists — do not only update this plan page.

---

## Explicit non-goals until reopened

- Replacing USB entirely  
- Full map GUI on handheld  
- Certified life-safety / evacuation system claims for the siren  
- Redesigning the 12-byte node PDU without a written budget impact
