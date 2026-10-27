---
tags:
  - slf-ewn
  - firmware
  - code
aliases:
  - Source
parent: "[[Firmware Hub]]"
---

# Code Hub

AVR-GCC C. **No Arduino core.** One `.c` / `.h` per device, coordinators only sequence work. When parts or paths disagree with older notes, **[[Project Overview#What we decided]]** wins — except where this page records a **shop override**.

← [[SLF-EWN Hub]] · Formats: [[Firmware Hub#Data formats]] · Pins: [[Transmitter Simulation]] · [[Receiver Simulation]] · Proposal: [[Full Proposal]] · Overview: [[Project Overview]]

---

## Repository layout

```
Code/
├── real/                     production sources (this page)
│   ├── Common/               shared 12-byte protocol
│   ├── Receiver/             tower coordinator + drivers
│   ├── Transmitter/          node coordinator + drivers
│   ├── ReceiverDevice/       standalone AVR project (single main.c)
│   └── TransmitterDevice/    standalone AVR project (single main.c)
└── sim/                      Proteus child firmwares
    ├── receiver/fake_lora/
    └── transmitter/
        ├── fake_accelerometer/
        └── fake_lora/
```

`Code/real/` is the modular production build. The two `*Device/` projects are self-contained CMake builds kept for bench work; they are **not** linked into the production images.

---

## Firmware decisions (as built)

| Topic | Decision | Where it lives |
|:------|:---------|:---------------|
| MCU / language | ATmega328P, bare-metal AVR-GCC C | All production `.c` |
| Node clock | 8 MHz internal RC | `transmitter*.c` `F_CPU` |
| Tower clock | 16 MHz crystal | `receiver*.c` `F_CPU` |
| Accelerometer | **ADXL345** (shop stock). Always-on 3.6 V rail so ACTIVITY/INT1 can wake the MCU; low-power wait between samples (~23 µA measure) | `transmitter_accelerometer.c` |
| Radio | **Ra-02 / SX1278**, full register drivers (not Arduino LoRa lib) | `transmitter_lora.c`, `receiver_radio.c` |
| RF air config | 433.0 MHz, SF7, BW 125 kHz, CR 4/5, explicit header, CRC on, sync `0x12`, preamble 8 | Both LoRa modules (must match) |
| Tower log | **W25Q64** SPI NOR, no FAT | `receiver_storage.c` |
| Flash wear policy | Append sequential records; **sector erase** on entry; **chip erase** when full then restart at 0 | `receiver_storage_append()` |
| Delays | **No `util/delay`** in production device modules; Timer0 CTC waits. (Tower `receiver.c` still uses `_delay_us` only for UART byte timeout poll.) | Node / radio / accel / storage |
| Sensor power | ZVP2106A gate **PD4**: rail on only for sample + TX | `transmitter.c` |
| Sleep | Power-down + WDT 8 s ticks; production target **15 min**; `SIM_FAST_WAKE 1` → 1 tick (~8 s) for Proteus | `transmitter.c` |
| Wake stagger | `T_i = 15 min + (id % 30) s`, quantised to 8 s WDT ticks; CRC drops residual overlaps | `transmitter.c` |
| Air frame | Fixed **12 bytes** + CRC-8 (poly-style `0x07`) | `Common/slf_protocol.*` |
| Flash record | **16 bytes** = 4-byte record id + 12-byte frame | Storage + Qt |
| Dashboard link | UART **115200**, poll commands `'S'`/`'D'`/`'N'`/`'C'`, replies start with **`0xAA`** | `receiver.c` |
| Default node ID | `0x0104` | `transmitter.c` |
| Proteus | Separate **fake** MCU firmwares under `Code/sim/`; **not** linked into production `.hex` | `fake_*.c` |

---

## What the code does

Two production boards, three Proteus-only child MCUs.

### Buried node (`Transmitter/`)

`transmitter.c` wakes on WDT or motion INT0, gates the sensor rail, reads moisture (ADC1) and tilt (ADXL345), builds the 12-byte frame, transmits on Ra-02, prints debug @ 9600, hibernates again.

Device modules:

| Module | Role |
|:-------|:-----|
| `transmitter_accelerometer.c/.h` | ADXL345 SPI Mode 3: DEVID `0xE5`, ±2 g full-res, 100 Hz, MEASURE; burst `DATAX0`…`DATAZ1` → `int16_t` axes |
| `transmitter_lora.c/.h` | SX1278 register TX: sleep→LoRa, Frf 433 MHz, PA_BOOST, FIFO load, mode TX, wait TxDone (DIO0 / IRQ flag) |

Pins (node):

| Signal | Pin |
|:-------|:----|
| LoRa NSS | PB1 |
| Accel CS | PB2 |
| SPI MOSI/MISO/SCK | PB3 / PB4 / PB5 |
| Motion INT (INT0) | PD2 |
| LoRa DIO0 TxDone (INT1) | PD3 |
| Sensor-rail gate (active low) | PD4 |
| UART debug TX | PD1 @ 9600 |

### Surface tower (`Receiver/`)

`receiver.c` serves the Qt dashboard over USB-UART, accepts radio frames, CRC-checks, appends to W25Q64.

Device modules:

| Module | Role |
|:-------|:-----|
| `receiver_radio.c/.h` | SX1278 continuous RX; DIO0=RxDone → INT0 reads FIFO into buffer |
| `receiver_storage.c/.h` | W25Q64: JEDEC check, append-only log, sector erase, chip erase on full / clear |

Pins (tower):

| Signal | Pin |
|:-------|:----|
| Flash CS | PB1 |
| Radio CS | PB2 |
| SPI MOSI/MISO/SCK | PB3 / PB4 / PB5 |
| Radio DIO0 RxDone (INT0) | PD2 |
| UART to FT232 / Qt | PD0/PD1 @ **115200** |

### Shared protocol

`Common/slf_protocol.c/.h` (include path `../Common/` from both boards):

```
SLF_FRAME_LEN        12
SLF_RECORD_SIZE      16
SLF_STATUS_EMERGENCY 0x80   (bit7 of status byte)
SLF_STATUS_ACCEL_ERROR 0x01
slf_crc8(data, len)         // init 0x00, poly 0x07 style
```

`SLF_LORA_CMD_FIFO_WRITE 0x8D` remains defined for **Proteus fakes only**. Production LoRa does **not** send `0x8D`; it uses SX1278 `RegFifo` writes (`addr | 0x80`).

---

## Chip hierarchy (what you flash)

```
Physical board / Proteus
├── Transmitter/
│   ├── transmitter.c                 8 MHz   production coordinator
│   ├── transmitter_accelerometer.c   8 MHz   ADXL345 driver
│   ├── transmitter_lora.c            8 MHz   SX1278 TX driver
│   ├── Common/slf_protocol.c         shared
│   ├── fake_accelerometer.c          8 MHz   Proteus ADXL345 slave only
│   └── fake_lora_transmitter.c       8 MHz   Proteus ANT bridge only
└── Receiver/
    ├── receiver.c                    16 MHz  production coordinator
    ├── receiver_radio.c              16 MHz  SX1278 RX driver
    ├── receiver_storage.c            16 MHz  W25Q64 driver
    ├── (Common via ../Common/)
    └── fake_lora_receiver.c          8 MHz   Proteus ANT bridge only
```

---

## Build groups

| Image | Sources | Flash to |
|:------|:--------|:---------|
| **Transmitter production** | `transmitter.c` + `transmitter_accelerometer.c` + `transmitter_lora.c` + `Common/slf_protocol.c` | Buried ATmega328P |
| **Receiver production** | `receiver.c` + `receiver_radio.c` + `receiver_storage.c` + `Common/slf_protocol.c` | Tower ATmega328P |
| Fake accelerometer | `fake_accelerometer.c` only | Proteus child |
| Fake LoRa TX | `fake_lora_transmitter.c` only | Proteus child |
| Fake LoRa RX | `fake_lora_receiver.c` only | Proteus child |

Do **not** link both coordinators into one image. Do **not** link any `fake_*.c` into production.

### Standalone device projects

`Code/real/ReceiverDevice/` and `Code/real/TransmitterDevice/` are self-contained AVR-CMake projects (`main.c` + `CMakeLists.txt`) used for bench bring-up. They duplicate coordinator logic for a single-image build and are **not** part of the modular `.hex` groups above.

---

## Module API surface

Headers expose operations only. SPI modes, CS pins, register addresses, and ISRs stay inside the matching `.c`.

| Device | Header API |
|:-------|:-----------|
| Protocol | `slf_crc8()` + frame/status constants |
| Accel | `transmitter_accelerometer_init()` · `transmitter_accelerometer_read(x,y,z)` → 1 if DEVID OK |
| LoRa TX | `transmitter_lora_init()` · `transmitter_lora_send(frame, len)` → 1 if TxDone |
| LoRa RX | `receiver_radio_init()` · `available()` · `copy_frame()` · `wait_idle()` |
| Flash | `init` · `count` · `capacity` · `append(frame)` · `read(id, record)` · `clear()` |

---

## SPI bus rules

| Board | Default SPI | Device exceptions |
|:------|:------------|:------------------|
| Node | Master **Mode 0**, `/16` | ADXL345 forces **Mode 3** for its CS window, then restores Mode 0 for SX1278 |
| Tower | Master **Mode 0** for flash and for **real** SX1278 | Proteus **fake** LoRa RX still expects Mode 1 clock-out (sim only) |

SX1278 always Mode 0 (`CPOL=0`, `CPHA=0`). Write = address `| 0x80`, read = address `& 0x7F`.

---

## 12-byte air frame

| Byte | Content |
|:----:|:--------|
| 0–1 | Device ID (big-endian), default `0x0104` |
| 2–3 | Moisture ×100 (0…10000 from 10-bit ADC scaled) |
| 4–5 | Tilt X (int16, accel raw) |
| 6–7 | Tilt Y |
| 8–9 | Tilt Z |
| 10 | Status: bit7 emergency, bit0 accel error, bits6–0 sequence |
| 11 | CRC-8 over bytes 0…10 |

### 16-byte flash record

| Byte | Content |
|:----:|:--------|
| 0–3 | Record id (big-endian, starts at 0) — first byte of a written record is always `0x00` so boot scan can find the first `0xFF` free slot |
| 4–15 | Copy of the 12-byte air frame |

Capacity: `0x800000 / 16` = **524 288** records on W25Q64.

---

## Node loop (as in `transmitter.c`)

1. Hibernate (power-down); WDT 8 s interrupt ticks; optional INT0 motion flag.
2. Wake when ticks ≥ interval **or** motion; the interval includes the ID stagger.
3. Accel to measure + clear ACTIVITY latch; gate **on** → 5 ms settle → ADXL345 read + ADC1 moisture → gate **off**.
4. Pack frame + CRC; set emergency / accel-error bits.
5. Gate **on** → 2 ms → `transmitter_lora_send()` → gate **off**; accel back to low-power wait.
6. UART debug line; back to hibernate.

Production interval: **15 minutes** (`PRODUCTION_INTERVAL_S`) plus `(DEVICE_ID % 30) s` stagger, quantised to 8 s WDT ticks → **896–928 s** (F2). With `#define SIM_FAST_WAKE 1` the interval is **1** WDT tick (~8 s) for bench/Proteus.

---

## Tower loop (as in `receiver.c`)

1. Init radio (continuous RX) + storage (JEDEC + scan for `record_count`).
2. INT0 on DIO0: radio driver fills internal 12-byte buffer.
3. Main: if frame available → CRC OK → `receiver_storage_append()`.
4. UART commands (single byte):

| Cmd | ASCII | Action |
|:---:|:-----:|:-------|
| `0x53` | `S` | Status: magic + cmd + length + count/free (8 B payload) |
| `0x44` | `D` | Full dump of all records |
| `0x4E` | `N` | New records after 4-byte last-id |
| `0x43` | `C` | Chip erase + reset count |

Every reply header: **`0xAA`**, command, 32-bit big-endian length, then payload.

---

## LoRa production path (SX1278)

**TX (`transmitter_lora.c`)**

1. Soft init: Sleep → set `LongRangeMode` → verify `RegVersion == 0x12`.
2. Standby; Frf = 433.0 MHz (`0x6C5333`); PA_BOOST; modem SF7/BW125/CR4/5; sync `0x12`.
3. DIO0 map = **TxDone**.
4. Send: Standby → `RegPayloadLength` → `RegFifoAddrPtr=0` → burst FIFO write → mode **TX** → wait TxDone (INT1 or poll `RegIrqFlags`) → clear IRQs → Standby.

**RX (`receiver_radio.c`)**

1. Same RF config; DIO0 map = **RxDone**.
2. Mode **RXCONTINUOUS**.
3. INT0: read `RegIrqFlags`; reject CRC error; `RegRxNbBytes` + `RegFifoRxCurrentAddr` → read FIFO → re-arm continuous RX.

Proteus without a Ra-02 model still uses the old fake chain: node SPI `0x8D`+12 B → fake TX UART ANT @ 9600 → fake RX → DIO0 → tower clocks 12 B (Mode 1). **That path is sim-only**; production `.hex` speaks real registers.

---

## Flash wear-leveling (`receiver_storage.c`)

NOR rule: bits only program **1→0**; need **4 KB sector erase** to get 1s back.

- No in-place header counter (would kill one sector in hours).
- Boot: binary/linear scan for first record whose first byte is still `0xFF` → that index is `record_count`.
- Append: if `record_count >= FLASH_MAX_RECORDS` → **chip erase** (`0xC7`), reset count; if address is sector-aligned → **sector erase** (`0x20`); page program 16 B; increment count.
- `clear()`: real chip erase, not a fake counter reset.

Spreads erase cycles across the full 8 MB instead of one hot sector (~100k cycles/sector per W25Q64BV datasheet).

---

## Accelerometer shop override

| | Overview ideal | **This firmware** |
|--|:---------------|:------------------|
| Part | LIS3DH (`WHO_AM_I` 0x33, ~2 µA) | **ADXL345** (`DEVID` 0xE5) |
| Why | Multi-year hibernation budget | Vector4Engineering shelf stock |
| Mitigation | — | FET gate; measure only tens of ms per wake |

SPI protocol for ADXL345: Mode 3, bit7=read, bit6=multi-byte, `DATAX0` burst. Fake accelerometer emulates the same map for Proteus.

If LIS3DH becomes available, only `transmitter_accelerometer.c` + fake need to change; coordinator API stays `init` / `read(int16_t*)`.

---

## Self-implemented firmware (supply-chain / trust policy)

**Policy.** Application and device drivers are **written in-tree**. Third-party Arduino libraries, vendor “.h + .c” packs, and random GitHub LoRa/accel helpers are **not** used. The only non-project headers are **toolchain / language standard** includes shipped with the AVR-GCC environment.

**Rationale (security / integrity).** External application headers and opaque library blobs can be altered (accidental fork drift, compromised mirror, or intentional malfunction). Keeping drivers under project review reduces that surface: SPI register maps, CRC, flash erase policy, and sleep behaviour are visible in `Code/`.

### Allowed includes (audit of `Code/real/` as built)

| Kind | Examples | Role |
|:-----|:---------|:-----|
| **Toolchain / ISO C** | `<stdint.h>`, `<string.h>` | Fixed-width types, `memcpy`-class helpers |
| **AVR libc / device** | `<avr/io.h>`, `<avr/interrupt.h>`, `<avr/sleep.h>`, `<avr/wdt.h>`, `<util/delay.h>` | SFRs, ISR, sleep, WDT; tower UART timeout uses `_delay_us` only |
| **Project-local** | `slf_protocol.h`, `transmitter_*.h`, `receiver_*.h` | Protocol and drivers owned by the team |

### Explicitly not used

| Rejected dependency | Why |
|:--------------------|:----|
| Arduino core / `.ino` wrappers | Hidden runtime, non-audited helpers |
| Popular LoRa Arduino libraries | Opaque TX path; RF settings not matched to our freeze list |
| Vendor ADXL / W25Q “library” packs | Prefer datasheet register drivers in-tree |
| FAT / SdFat / filesystem stacks | Tower uses raw NOR records only |

### What we implemented ourselves

| Module | Responsibility |
|:-------|:---------------|
| `Common/slf_protocol.*` | 12-byte frame layout, CRC-8, status bits |
| `transmitter_accelerometer.*` | ADXL345 SPI Mode 3, low-power ACTIVITY, measure mode |
| `transmitter_lora.*` | SX1278 register TX (not FIFO-only fake contract on hardware) |
| `receiver_radio.*` | SX1278 continuous RX, DIO0 / FIFO read |
| `receiver_storage.*` | W25Q64 append, sector/chip erase policy |
| `transmitter.c` / `receiver.c` | Sleep, FET rail, host poll protocol |
| `fake_*.c` | Proteus-only SPI/UART stand-ins — **not** linked into production `.hex` |

**Honest limit.** Trust still depends on the **compiler, libc, and chip datasheets** being genuine. This policy does not claim formal binary attestation; it claims **no third-party application middleware** in the node/tower firmware path.

---

## Delays policy

| Location | Mechanism |
|:---------|:----------|
| Node coordinator, accel, LoRa TX | Timer0 CTC 1 ms ticks (`node_wait_ms` / local copies) |
| Tower radio / storage | Timer0 CTC where needed; BUSY poll on flash |
| `receiver.c` UART read timeout | `_delay_us(10)` poll loop (only remaining library delay) |
| `fake_*.c` | May use `util/delay` for Proteus stability |

---

## Transmission chain (end-to-end)

**Production (real Ra-02 both ends)**

1. Node packs 12 B → SX1278 FIFO TX @ 433 MHz SF7.  
2. Through-soil / air LoRa.  
3. Tower SX1278 RxDone → FIFO read 12 B.  
4. CRC → append 16 B record on W25Q64.  
5. Qt polls UART 115200 (`S`/`D`/`N`/`C`); replies `0xAA`….

**Proteus (no Ra-02 model)**

1. Node SPI `0x8D` + 12 B → fake LoRa TX.  
2. Fake TX UART 12 B @ 9600 (ANT).  
3. Fake RX CRC → DIO0 → tower SPI Mode 1 clock-out 12 B.  
4–5. Same storage + Qt path.

Same 12-byte payload either way; only the radio device modules differ.
