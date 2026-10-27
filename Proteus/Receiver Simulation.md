---
tags:
  - slf-ewn
  - proteus
  - receiver
parent: "[[Proteus Hub]]"
---

# Receiver sheet — pin map

5 V USB, 16 MHz, **no FET**. Masters: [[receiver.c]]. Fake radio: [[fake_lora_receiver.c]]. Both sheets: [[Proteus Hub#End-to-end test]]. PCB: [[Receiver PCB]].

Fake LoRa = extra ATmega328P. Flash = **25AA1024** EEPROM stand-in. USB = **COMPIM**. The EEPROM needs no erase; the production driver still issues NOR sector-erase commands, which the stand-in ignores.

---

## MCU → fake LoRa RX (parent)

| Master pin | Net | Fake RX pin | Role |
|:-----------|:----|:------------|:-----|
| PB2 `RADIO_CS_PIN` | CS | NSS | SPI chip select |
| PB3 MOSI | MOSI | MOSI | SPI incoming data |
| PB4 MISO | MISO | MISO | SPI outgoing (output only) |
| PB5 SCK | SCK | SCK | SPI clock |
| PD2 `RADIO_DIO0_PIN` | INT0 | DIO0 | RX frame ready |

Slave UART RX = ANT link at **9600 baud**, 12 binary bytes per frame. 
SPI **Mode 1** (CPHA=1) for the fake. Production `receiver_radio.c` uses Mode 0 for the real SX1278 (F5). After CRC valid, DIO0 stays high until 12 SPI clocks transfer the frame. See [[Proteus Hub#Fake LoRa RX (`fake_lora_receiver.c`)]].

---

## MCU → 25AA1024 EEPROM (parent)

| Master pin | Net | EEPROM pin | Role |
|:-----------|:----|:-----------|:-----|
| PB1 `FLASH_CS_PIN` | CS | CS (pin 1) | SPI chip select |
| PB3 MOSI | MOSI | SI (pin 5) | SPI data in |
| PB4 MISO | MISO | SO (pin 2) | SPI data out |
| PB5 SCK | SCK | SCK (pin 6) | SPI clock |
| — | — | WP (pin 3) | 10 kΩ → VCC (write protect off) |
| — | — | HOLD (pin 7) | 10 kΩ → VCC (hold off) |

SPI **Mode 0**. Stores records at `0x000000` (record 0), 16 bytes each — no header. Record ID is appended to each frame at flash write time. See [[Firmware Hub#Surface tower (`receiver.c`)]]. Stands in for W25Q64 SPI NOR flash on production PCB.

> **Sim caveat.** The production driver checks JEDEC ID `EF 40 17` and uses NOR sector erase (`0x20`). The 25AA1024 is an EEPROM with neither the Winbond ID nor sector erase, so the simulation only logs if the sim build bypasses the JEDEC check (or the model is patched). Verify which build the Proteus sheet uses before treating flash append/dump as demonstrated.

---

## COMPIM (serial bridge to Qt dashboard)

Stands in for FT232RL USB-to-serial chip on the production PCB.

| MCU pin | COMPIM | Function |
|:--------|:-------|:---------|
| PD1 TXD | RXD of dashboard COMPIM | Transmit to dashboard **115200** |
| PD0 RXD | TXD of dashboard COMPIM | Receive commands from dashboard |
| — | DTR / RTS | Assert both (some libraries stall without) |
| — | Second COMPIM (pair) | Receive ANT data from fake RX (9600 baud) |

**Two separate COMPIM components:**
1. **Dashboard link** (115200): MCU ↔ Qt dashboard (PD1 TX, PD0 RX)
2. **Antenna link** (9600): Fake LoRa RX → MCU receive path (ANT packet stream)

16 MHz crystal + 2× 22 pF load caps. 10 kΩ RESET pull-up. 100 kΩ on `RADIO_CS` and `FLASH_CS`. 10 µF + 100 nF on USB 5 V rail. 100 nF AREF.

On production PCB, both links are 3.3 V and go through an FT232RL. Proteus uses two independent COMPIMs at 5 V (logic tolerates it). Remove the LM1117 3V3 regulator when real 3.3 V radio and flash parts are substituted in hardware.

---

## Design notes

- Record ID is **`record_count`**, not a millisecond timestamp. Fake RX does not stamp.
- Fake RX waits for **12 binary bytes** then CRC — overlapping TX sheets corrupt the 12-byte window (`[CRC FAIL]`).
- Dashboard must **poll**; nothing is pushed on UART when a LoRa frame arrives.

### Support parts (Proteus library)

| Part | Keyword | Value | Where |
|:-----|:--------|:------|:------|
| RESET pull-up | `RES` | 10 kΩ | PC6 → 5 V |
| Crystal | `CRYSTAL` | 16 MHz | XTAL1–XTAL2 |
| Load caps | `CAP` | 22 pF ×2 | each XTAL pin → GND |
| CS pull-ups | `RES` | 100 kΩ | `RADIO_CS`, `FLASH_CS` → 5 V |
| WP / HOLD | `RES` | 10 kΩ | 25AA1024 pins 3 and 7 → 5 V |
| ANT pull-down | `RES` | 10 kΩ | ANT → GND |
| Decouple | `CAP` | 100 nF | VCC/AVCC, AREF |
| USB bulk | `CAP-ELEC` | 10 µF | USB 5 V and near MCU VCC |
