---
tags:
  - slf-ewn
  - proteus
  - transmitter
parent: "[[Proteus Hub]]"
---

# Transmitter sheet — pin map

3.6 V, 8 MHz. Master hex: [[transmitter.c]]. Fakes: [[fake_lora_transmitter.c]], [[fake_accelerometer.c]]. How to run: [[Proteus Hub]]. PCB: [[Transmitter PCB]].

---

## Master pin map (one table)

| MCU | Net | Goes to |
|:----|:----|:--------|
| PC6 RESET | — | 10 kΩ → VCC |
| PD2 INT0 | `INT0` | Fake ADXL345 INT1 (motion wake) |
| PD3 INT1 | `INT1` | Fake LoRa DIO0 (TX done) |
| PD4 | `GATE_BIT` | ZVP2106A gate (LOW = sensors on) |
| PB1 | `LORA_NSS_PIN` | Fake LoRa NSS (radio select) |
| PB2 | `ACCEL_CS_PIN` | Fake ADXL345 CS (accel select) |
| PB3 MOSI | `MOSI` | Both SPI slaves |
| PB4 MISO | `MISO` | Both (slaves tri-state when CS high) |
| PB5 SCK | `SCK` | Both |
| PC1 ADC1 | `SOIL_RAW` | POT-HG wiper |
| PD1 TXD | — | Debug terminal 9600 |
| AVCC | `VCC` | 3.6 V |

### MOSFET ZVP2106A

Gate `PD4` + **100 kΩ** to VCC (stays OFF during reset). Source = always-on VCC. Drain = `VCC_SW`.

**VCC_SW loads only:** moisture (POT-HG top) **and** fake/real **radio VCC**.  
**Not on VCC_SW:** MCU, **ADXL345** — stay on always-on 3.6 V so motion INT can fire during sleep.

Master turns VCC_SW on for measure / TX, off for hibernate. For the SPI `0x8D` TX path, rail is on for the radio burst.

### Moisture POT-HG

Top = `VCC_SW` · Wiper = `SOIL_RAW` · Bottom = GND.  
Probe/sim: dry ≈ slider **100%** (~3.6 V, high ADC). Wet ≈ **0%** (~1.0–1.5 V).  
`transmitter.c` maps `moisture = adc * 10000 / 1023` **without invert**, so a wet pot currently packs a **low** %. Fix in firmware or call it out in the report (**F3**).

### Passives

100 kΩ on `CS_LORA`, `CS_ACCEL`. Pull as required on `INT0`. **47 mF / 5.5 V** supercap on always-on VCC–GND; **1.2 kΩ** series from cell to rail (inrush ≤ 3 mA) (not a 3.6 V memory-backup cap). 100 nF on always-on near MCU/ADXL; 100 nF on `VCC_SW` near radio; AREF as needed.

BOD disabled (`0xFF`) or 1.8 V (`0xFE`) — the Arduino 2.7 V (`0xFD`) BOD dies on a sagging Li-SOCl₂ cell.

### Placement

- Gate pull-up **on the PD4 → gate** net.
- Supercap at the **battery terminal** on the always-on rail (ready for the radio when FET turns on).
- ADXL VCC tied to **always-on**, not `VCC_SW`.

---

## Master → fake LoRa transmitter (parent)

| Master pin | Net | Fake TX pin | Role |
|:-----------|:----|:------------|:-----|
| PC6 RESET | RESET | — | 10 kΩ to 3.6 V |
| VCC / AVCC / GND | — | — | 3.6 V rail |
| PB1 `LORA_NSS_PIN` | NSS | NSS | active-low chip select |
| PB3 MOSI | MOSI | MOSI | SPI commands + payload |
| PB4 MISO | MISO | MISO | child → parent **Output/BIDIR** |
| PB5 SCK | SCK | SCK | SPI clock |
| PD3 `LORA_DIO0_PIN` | INT1 | DIO0 | 2 ms pulse = TX complete |

---

## Inside the fake LoRa transmitter child

| Slave pin | Internal net | Outer pin | Direction | Purpose |
|:----------|:-------------|:----------|:----------|:--------|
| PB2/SS | NSS | NSS | IN | master selects radio |
| PB3 MOSI | MOSI | MOSI | IN | SPI incoming data |
| PB4 MISO | MISO | MISO | BIDIR | SPI outgoing; **release when NSS high** |
| PB5 SCK | SCK | SCK | IN | SPI clock |
| PD3 | DIO0 | DIO0 | OUT | 2 ms pulse after ANT transmit |
| PD1 TXD | ANT | COMPIM / terminal | 9600 | 12 raw bytes **binary, not ASCII** |
| (optional) | ANT | 10 kΩ → GND | | no floating net |

Slave NSS is **PB2**. Master SPI command `0x8D` + 12 payload bytes, `_delay_us(250)` each.
ANT is **pure binary**. See [[Proteus Hub#Fake LoRa TX (`fake_lora_transmitter.c`)]].

---

## Master → fake ADXL345 accelerometer (parent)

| Master pin | Net | Accel pin | Role |
|:-----------|:----|:----------|:-----|
| PB2 `ACCEL_CS_PIN` | CS | CS | active-low chip select |
| PB3 MOSI | MOSI | DIN | register address / command |
| PB4 MISO | MISO | DOUT | register data bytes |
| PB5 SCK | SCK | SCLK | SPI clock |
| PD2 INT0 | `INT0` | INT1 | motion interrupt (active-high) |
| Always-on VCC | — | VCC | **Not** FET-gated (INT in sleep) |
| GND | — | GND | — |

---

## Inside the fake ADXL345 accelerometer child

| Slave pin | Net | Direction | Purpose |
|:----------|:----|:----------|:--------|
| PB2/SS | CS | IN | select chip |
| PB3 MOSI | DIN | IN | write register address, read bit in MSB |
| PB4 MISO | DOUT | BIDIR | data bytes; release when CS high |
| PB5 SCK | SCLK | IN | Mode **0** clock |
| PD2 INT0 | INT1 | OUT | asserts when any axis magnitude **> 32 counts** from center |
| VCC | always-on VCC | — | Stays powered in sleep for motion INT |
| AVCC | inner VCC | — | ADC reference |
| GND | GND | — | — |

Axes: **RV1–RV3** (POT-HG) on **ADC0 / ADC1 / ADC2** 
  - `PC0` → acceleration X
  - `PC1` → acceleration Y
  - `PC2` → acceleration Z
  
Each mapped to 8-bit value (center `0x80` = 0 g). Sampled every 150 ms.
