---
tags:
  - slf-ewn
  - presentation
  - plain-language
aliases:
  - Slide explanations
  - Non-technical guide
parent: "[[Challenges and Solutions]]"
status: active
---

# Challenges and Solutions — plain-language notes

These notes sit **beside** [[Challenges and Solutions]]. Each heading matches a slide code (A1, B2, …).  
They do **not** repeat the slide. They explain, in everyday language, **why** each idea exists and **where** the numbers and choices came from.

The deck covers **Phase 1 challenges and solutions**; Phase 2/3 appear only as a short look-ahead.

Most technical slides open with a short paragraph on the slide itself: *why this is a challenge* and *which direction the solution takes*. Those lines are the starting point; the tables and equations carry the proof. These notes stay in plain language for non-technical listeners and do not repeat every equation.

---

## Opening slides

**Title (SLF-EWN)** — Names the project and states the review style: every hard choice shows a conflict, some numbers, what was built, and what is still unproven.

**How this deck is organised** — Roadmap of sections A–J so a non-specialist can follow the journey: system → radio packet → why this radio → location logic → battery → wake-up → antenna → board size → memory/security → what is still open.

---

## A1

The picture on this slide is the whole product in four boxes.

A small computer and sensors are **buried about a metre deep**. They send a short radio message through the soil to a **tower on the surface**. The tower keeps a log even if the laptop is unplugged. A **desktop app** is where people look at maps, zones, and alarms.

Nothing “magic” is in the radio message except who spoke and the latest moisture and movement readings. Place names and map pins live on the computer, because the buried device must save every bit of battery and keep the radio message tiny.

---

## A2

Each part of the chain has a different job and a different power story.

The buried unit runs slowly and sleeps most of the time so the battery can last years. The tower stays more “awake” because it must catch messages and talk to the PC. The PC only asks the tower for data when the operator wants it — the tower does not stream endlessly.

Also: the **simulation** on the desk may pretend the radio chip is simpler than the real chip in the field. That is fine for testing software, but the real boards must use the real radio driver, not the pretend one.

---

## A3

These are the non-negotiable limits that forced the design.

- The capsule should work for **years without being dug up** → special long-life battery and extreme power saving.  
- Sensors sit about **one metre underground** → the radio must punch through soil, and the sensors see moisture and movement where slopes actually weaken.  
- Student-friendly **one-layer circuit boards** → no fancy multi-layer manufacturing.  
- A small, common microcontroller → simple software, fixed message formats.  
- Short radio messages → less energy and fewer collisions when several nodes share the air.  
- Tower must remember data if the PC is away → onboard flash memory, not a USB stick file system.

If someone asks “why not Wi-Fi / a big battery / GPS on every node?”, the answer usually starts in this list.

---

## A4

How the team wants every problem to be judged:

1. What two needs fight each other?  
2. What number or physical limit decides?  
3. What did we actually build?  
4. What is still only theory until we measure it?

That is why later slides keep saying “designed” or “open” — honesty about what is proven.

---

## B1

People naturally want the buried sensor to send *everything*: battery voltage, temperature, GPS, long text status.  

That fights the battery and the soil. A longer message stays on the radio longer, uses more energy, collides more easily with neighbours, and is harder to receive after the signal has been weakened by wet earth.

So the project **froze a very short fixed message**. Rich maps and history stay on the laptop.

---

## B2

The short message is twelve bytes — roughly the size of a short text fragment, not a photo.

It only carries: which node is talking, how wet the soil reading is, movement on three axes, a small status flag (including “this wake was an emergency”), and a checksum so garbage can be thrown away.

Where it came from: the firmware packing code and the need to keep radio time short. The emergency flag exists so a sudden jolt can look different on the dashboard from a normal timed sample.

---

## B3

The same twelve bytes appear in three wrappers:

- On the radio, alone.  
- In the tower’s memory, with an extra counter so the operator can say “give me everything after record 1200.”  
- On the USB link to the PC, inside a command/reply envelope so the PC can ask for status, full dump, or “only new records.”

If encryption is added later, the message may grow; then energy and storage math must be redone.

---

## C1

Why not the same radios phones use (around 2.4 GHz)?

Wet soil soaks high frequencies more aggressively. Marketing claims of “kilometres of LoRa” assume open air. Here the first metre is **underground**.

Published engineering ranges for wet clay at lower frequencies are on the order of **tens of decibels of loss per metre** — meaning a large fraction of the signal can disappear before it even reaches the open air. That is why the project picked **433 MHz class** radios and only claims about a metre through soil plus a limited distance after the signal breaks out.

---

## C2

“433 MHz” is the channel band. **LoRa** is a way of shaping the signal so a very weak transmission can still be understood, at the cost of sending data slowly.

That trade is perfect here: we only send a tiny packet every many minutes. We need the receiver to hear a faint signal after soil loss. We do not need video or phone-style networking.

The concrete module (Ra-02 / SX1278) is what the team can buy and drive from the small microcontroller over SPI.

---

## C3

This is the “radio settings shopping list” the two ends of the link must agree on: frequency, spreading, bandwidth, power class, antenna height and orientation, and a preference that each transmission not linger too long on air.

The formula on the slide is the standard idea: received power = what is transmitted + antenna gains − losses in soil, air, and extras. Until we measure loss in **our** soil with **our** antenna, the range numbers stay design targets, not certified facts.

---

## D1

Operators think in **places** (“north pit wall”, “village slope”). The radio only carries a **numeric ID**.

That split is deliberate. Zone names, map polygons, and categories would bulk up every transmission. Instead, the laptop database attaches human meaning to each ID. Moving a pin on the map does not require changing underground firmware.

---

## D2

After a real landslide, the capsule may shift or tip. The system still needs a continuous story for “this sensor’s history.”

So the **ID never changes**. History stays under that ID. Humans update the map pin after a survey if the device moved. There is **no GPS chip** underground — displacement is not calculated on the node; only motion detection and continuous identity.

---

## D3

Why bury moisture and movement sensors instead of only watching the surface?

Slopes often weaken when water builds up **inside** the ground and when the mass starts to creep or suddenly accelerate. A rain gauge on a roof can miss that. Depth around one metre is both a useful place to sense and the same depth the radio must cross.

This prototype does **not** replace a full geological computer model. It is an early-warning instrument: wetter + more movement → raise attention, with thresholds people can adjust.

---

## D3b

Why not a cheap **resistive** soil moisture probe (two bare metal legs measuring resistance)?

In wet soil those legs form a small battery-like circuit: current flows through the water, the metal slowly dissolves or grows deposits (**electrolysis and corrosion**). Readings drift, the probe can open or short, and a sealed underground node cannot be cleaned every few months. That fights the whole idea of a **multi-year buried** instrument.

---

## D3c

**Capacitive** probes sense how the soil's electrical insulation (dielectric constant) changes as it gets wetter. The sensing metal is behind insulation, so there is no continuous DC current path through the pore water the way a resistive fork has.

Extra project choices that help the same problem:

- The probe is powered only when measuring (FET switch), not for the whole 15 minutes.
- Electronics live in a sealed capsule.
- Software can calibrate and fix inverted wet/dry readings.

So capacitive is chosen for **longevity in wet soil**, not because it is a perfect lab water-content meter. Thresholds on the PC still treat it as a practical **wetness indicator**.

---

## D4

A mine wall and a gentle hillside should not share one blind alarm number.

Defaults exist (moisture and tilt bands, and “emergency motion wake always critical”), but each node can be overridden in the software. Categories and map zones help organise that. Changing an alarm on the PC does not require digging up the sensor.

---

## E1

The battery chemistry chosen for long underground life is excellent at lasting years, but poor at delivering big bursts of current. The radio, when transmitting, wants a large gulp of current for a short moment.

If the radio or moisture sensor stayed powered all day “just in case,” the years of life would vanish. Everything that follows in section E is about surviving that conflict.

---

## E2

Rough energy in the cell and a hard ceiling on average current for multi-year life (tens of microamps). The design aims even lower to leave margin for cold, ageing, and small leaks on the circuit board.

Practical extras: don’t use a brown-out setting that bricks the chip as the cell ages; use a simple jumper so storage does not drain the cell.

These numbers come from battery capacity and calendar arithmetic, not from marketing slides.

---

## E3

If the device is only fully awake for a tiny fraction of each quarter hour, the energy used while awake is almost free compared with what it uses while sleeping.

So the real enemy is **anything that draws current during sleep**. That is why later slides obsess over switching loads off and measuring true sleep current on hardware.

---

## E4

A small transistor switch cuts power to the moisture sensor and the radio when they are not needed. The microcontroller pin turns that rail on only for a short measure-and-send window.

This is ordinary power-gating, chosen in a through-hole part that matches the team’s simple board process. It is the main reason those hungry parts do not bleed the battery for fifteen minutes between samples.

---

## E5

Even with average current under control, the radio’s short high-current pulse can sag a weak battery line.

A **47 mF / 5.5 V** supercapacitor sits on the always-on rail, charged from the cell through a **1.2 kΩ** resistor. The radio briefly draws from the capacitor; the cell only sees a few milliamps of charge current. Math: about 127 mV dip for a 120 mA × 50 ms pulse, full recharge in roughly 5 minutes—well inside the 15-minute wake cycle. A tiny electrolytic would collapse the rail; a 2.7 V “memory backup” cap is the wrong voltage rating on a 3.6 V cell.

Still required: **measure** real sleep current on the real board; turn off the “fast wake” flag used only in simulation.

---

## F1

If the node only woke on a timer every fifteen minutes, a fast failure between samples could be invisible until the next tick.

So the design wants a second path: the motion sensor can **interrupt** the sleeping computer. That fights the urge to power *everything* down. The compromise is: switch off moisture and radio hard; leave the accelerometer able to raise an alarm line.

---

## F2

Two ways to wake:

- Timer → normal sample (trends, routine dashboard).  
- Motion interrupt → same sample, but flagged so the software shows critical.

Simulation speeds up time for lab testing; production must use the real long interval. The exact sensitivity and current of the accelerometer in that special low-power mode still need lab proof on real chips.

---

## G1

Soil eats radio energy. Using published ranges for wet ground, the first metre alone can cost a large number of decibels — like dimming a lamp enormously — before the signal reaches open air.

Design answers, in order: lower frequency, keep the antenna out of clay, point polarisation the same way as the tower, raise the tower antenna, don’t stay on air longer than needed. Until a field or barrel test, this remains informed estimation.

---

## G2

A radio antenna has a natural size related to wavelength. In free air at 433 MHz that is on the order of sixteen centimetres for a quarter-wave style element.

Packed in wet clay, the electrical size collapses and wanders as moisture changes — worst at the moment warnings matter most. So the antenna must sit in an **air pocket** inside the capsule, not mud-packed against the soil.

---

## G3

The practical shape is a small **helix** (coil) of copper that still uses about that free-air length of wire, short enough to fit under the top cap in air. It feeds through a normal SMA connector on the board.

---

## G4

If the buried antenna and the tower antenna are oriented differently (for example one sideways, one upright), part of the already-weak signal is wasted. Both are specified **vertical**, capsule installed level, antenna at the top of the stack in air.

---

## G5

The circuit board can interfere with its own radio: fast digital edges and supply spikes couple into the antenna path on a crowded single-layer board.

Mitigations are layout habits: short antenna path, local capacitors, keep the moisture sensing track away from the radio, ground copper where possible, plug-in radio module for clean placement. These are not cosmetic — they are part of the link budget on a tiny board.

---

## H1

The board must fit a tube that can be installed in a drilled hole: about 50 mm square board, capsule diameter a bit larger, overall length like a long bottle. Round tube preferred so soil pressure does not crack corners the way a box might.

---

## H2

Only **one** copper layer is allowed by project rules (cost, student fab, repair). Crossings use wire jumpers. Wide tracks for power, careful decoupling, chip in a socket so it is programmed in the lab before burial. That is process discipline as much as electrical theory.

---

## H3

Physical placement on the board: keep fast SPI and radio parts close together; keep the sensitive moisture analogue line quieter and apart; keep battery, supercap, and switch able to feed the radio pulse without starving the chip.

---

## I1

The tower must remember messages when the PC is gone. A full file system on an SD card is heavy for this small processor.

Instead: industrial-style SPI flash chip, fixed-size records (counter + the twelve-byte message), append until erase is needed. Capacity math shows room for a long service life for a small fleet if wear is not concentrated on one tiny region of the chip.

---

## I2

Several buried nodes share one radio channel. If they all transmit at the same instant, the tower hears noise and drops the checksum.

The intended fix is a small time offset based on each node’s ID so they don’t always align. Each node adds up to 29 seconds based on its ID (`15 min + id mod 30`) so wakes do not all line up. Residual overlaps still fail CRC and are dropped. Production must turn off the fast simulation wake flag.

---

## I3

Programming the buried board with a 5 V lab tool is a bad idea on a 3.6 V design, and the programming header wastes scarce space.

So chips are programmed in a socket in the lab, fuses set, then inserted into the board. No field reprogramming header on the buried assembly. The tower’s crystal fuse recipe still needs a written checklist so the first tower chip is not bricked.

---

## I4

Anyone with a suitable radio could, in principle, listen to clear messages: which node, how wet, whether an emergency flag fired.

The plan is to encrypt on the node and decrypt on the tower, using keys installed when the chip is programmed. Limits: tiny computer, tiny message, no internet certificate authority underground. This is **planned**, not finished.

---

## I5

From a **security** angle, another risk is the software itself: if the project depends on random Arduino or vendor library headers, those files could be changed (or come from an untrusted copy) and make the radio, sensors, or memory behave wrongly without the team noticing.

**Challenge:** third-party device libraries are hard to audit and can be altered.

**Solution:** write the drivers and the 12-byte protocol **inside the project**. Only normal compiler headers (`avr/io.h`, `stdint.h`, and similar) are imported. No Arduino core and no external LoRa or accelerometer packs.

That does not replace encryption of the radio message (still planned). It means the **code path on the node and tower** is under team control — see [[Code Hub#Self-implemented firmware (supply-chain / trust policy)]].

---

## J1

After all the design solutions, this is the **honest list** of what still weakens a Phase 1 claim: sleep current not measured, soil radio path not measured, simulation fast-wake still on, moisture polarity, motion-interrupt tuning, missing boards, tower fuse recipe, capsule drawings, and air encryption still open.

TX stagger is **already in code**. Each ID should match [[Status and open issues]] so slides and the live list do not diverge.

---

## J2

How to **finish Phase 1** in practice: turn off fast simulation wake; measure microamps in sleep; calibrate moisture; prove the accelerometer interrupt on hardware; fabricate boards; check antenna feed and noise; run a soil or pit radio test; then show a full path from node to tower flash to USB to the desktop app, including an emergency flag.

“Good enough” for Phase 1 is one node, one tower, clear SAFE/WARNING/CRITICAL behaviour—not Phase 2 backhaul or handhelds.

---

## J3

**Only a short look forward.** Phase 2 later completes the tower's field power (panel, charge controller, autonomy testing), adds a local siren, tower health on the host, a second LoRa for backhaul, a simple handheld, and registering towers and nodes on the PC. Phase 3 is richer sensors, mesh, and harder production features.

This challenges presentation stays about **Phase 1** problems and solutions. Detailed Phase 2 steps are in [[Phase 2 and Phase 3 plan]].

---

## J4

Closing arguments: sub-GHz LoRa for wet soil; short fixed messages; cell plus supercap plus switched loads plus motion wake; RF layout and air-pocket antenna; remaining work is measure and fab; self-written firmware for trust; Phase 2/3 mentioned only lightly.

---

## J5

Pointers into the vault for anyone who wants depth after the talk: Overview, Firmware, Code, Dashboard, Status, Phase plan, Data path, Proposal, Mentor readiness.

---

## End

Questions. Default to a **Phase 1** measurement or open-issue ID unless the question is about the future roadmap.
