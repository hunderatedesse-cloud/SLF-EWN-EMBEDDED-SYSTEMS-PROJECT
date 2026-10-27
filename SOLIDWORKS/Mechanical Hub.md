---
tags:
  - slf-ewn
  - mechanical
  - solidworks
  - blender
  - hub
aliases:
  - SOLIDWORKS
  - Enclosure
  - Mechanical
  - Blender
  - Animation
parent: "[[SLF-EWN Hub]]"
status: active
---

# Mechanical — capsule + film

Team **B** owns millimetres (SolidWorks). Team **D** owns renders (Blender) and **must not** invent new dimensions.

← [[SLF-EWN Hub]] · Board size: [[Transmitter PCB]] · [[Receiver PCB]] · RF: [[Project Overview#Radio]]

---

## Capsule (SolidWorks)

### Why a cylinder

A buried rectangle concentrates load at corners and faces; freeze–thaw opens cracks. A round tube spreads soil pressure, fits an **auger** hole, and matches an inline PCB stack.

| Spec | Value |
|:-----|:------|
| Inner diameter | **75–80 mm** (clears the **70.7 mm diagonal** of the 50×50 mm TX PCB + SMA) |
| Length | ~120 mm (PCB 50 mm + cell/air pocket + caps) |
| Wall | 3–5 mm |
| Rating | IP68-class seals (O-rings, epoxy at probe exit) |

Typical shop pipe: ~**75–80 mm ID × 120 mm** (a 50 mm ID tube cannot fit a 50×50 mm board).

### Stack (print this)

```
Top cap (gasket, removable)
├─ Right-angle SMA + air pocket for 433 MHz helix
├─ Ra-02 on 2.54 mm headers
├─ ATmega328P in DIP-28 socket
├─ ADXL345 + ZVP2106A + decoupling
├─ Supercap 47 mF / 5.5 V + 1.2 kΩ series
├─ Li-SOCl₂ AA (or 2/3A)
├─ Moisture blade through bottom cap into soil
Bottom cap (permanent after potting)
```

Helix: ~17 cm wire, ~3.4 cm wound height (10–11 turns, 5 mm mandrel), **in air**, coil axis **vertical**.

Inside the sealed volume: PCB traces and soldered probe wires only — **no extra connectors**.

### Waterproofing

- Thick-wall PVC, solvent-weld or threaded caps  
- Silicone O-rings; removable top, permanent bottom after potting  
- Cable gland or epoxy at moisture-blade exit  
- Conformal coat on the assembled PCB  

**Print features:** gasket groove, probe gland, air cavity for the helix, clearance for SMA + 50×50 mm board.

**Tower:** weather box for RX board (95×70 mm), antenna on a **1.5–2 m** pole, high-gain omni, vertical polarisation matching the buried helix.

### Source files

| File | Contents |
|:-----|:---------|
| `SOLIDWORKS/Part1.SLDPRT` | enclosure part 1 — solid model |
| `SOLIDWORKS/Part1.SLDDRW` | dimensioned drawing of part 1 |
| `SOLIDWORKS/Part2.SLDPRT` | enclosure part 2 — solid model |
| `SOLIDWORKS/Part2.SLDDRW` | dimensioned drawing of part 2 |

Native SolidWorks files; export STEP/STL for Blender. Dimensions above are the reference — do not re-scale the CAD to fit prose.

### SolidWorks deliverables

- [ ] Printable TX capsule (caps, gasket groove, probe gland)
- [ ] Tower housing
- [ ] STEP/STL that Blender can import
- [ ] Dimensioned drawing for the report

---

## Film (Blender)

Import the SolidWorks / EasyEDA model. **Do not** change millimetres here.

1. Export OBJ/STEP from EasyEDA or SolidWorks.  
2. Scale check: TX board **50 × 50 mm**. Capsule ID **75–80 mm**, length **~120 mm**, wall **3–5 mm**.
3. Node centroid at **1 m** depth; tower antenna feed at **1.5–2 m**.  
4. Helix **in an air pocket**, coil axis **vertical** — optional still: bare wire in clay (detuned) vs pocket (correct).  
5. Five nodes, unique positions, **one** tower; no mesh, no phone client.  
6. Cycles for report stills; Eevee for preview.

### Shot list

| Shot | Show |
|:-----|:-----|
| Burial cutaway | Tube, PCB stack, moisture blade in soil |
| Grid | Five nodes + tower, 100–200 m surface idea |
| Antenna | Helix in air vs clay |
| Scale | Hand / AA cell next to 50 mm board |
| Tower | Weather box, pole, omni whip |
| Why cylinder | Auger hole vs rectangle cracking at corners |

Story: moisture + tilt underground → 433 MHz through soil (~10–40 dB in the first metre) → tower W25Q64 → USB → Qt dashboard.
