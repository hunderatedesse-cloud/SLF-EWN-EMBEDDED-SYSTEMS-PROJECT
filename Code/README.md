# Code

AVR-GCC C, no Arduino core. Full behaviour and design notes: [[Code Hub]].

```
Code/
├── real/                 production firmware (flashed to the boards)
│   ├── Common/           shared 12-byte frame + CRC-8
│   ├── Receiver/         tower coordinator + SX1278 RX + W25Q64 storage
│   ├── Transmitter/      node coordinator + ADXL345 + SX1278 TX
│   ├── ReceiverDevice/   self-contained AVR-CMake bench project
│   ├── TransmitterDevice/ self-contained AVR-CMake bench project
│   ├── Code Hub.md       design notes and module APIs
│   └── realDevicePins.md datasheet pinouts
├── sim/                  Proteus-only child firmwares
│   ├── receiver/fake_lora/
│   └── transmitter/
│       ├── fake_accelerometer/
│       └── fake_lora/
└── README.md             this file
```

## Build groups (production)

| Image | Sources |
|:------|:--------|
| Transmitter | `real/Transmitter/transmitter.c`, `transmitter_accelerometer.c`, `transmitter_lora.c`, `real/Common/slf_protocol.c` |
| Receiver | `real/Receiver/receiver.c`, `receiver_radio.c`, `receiver_storage.c`, `real/Common/slf_protocol.c` |

Both `Receiver/` and `Transmitter/` include the shared protocol with `#include "../Common/slf_protocol.h"`.

Never link both coordinators into one image. Never link a `fake_*.c` into a production `.hex`.

## Standalone projects

Each of `real/ReceiverDevice/`, `real/TransmitterDevice/`, `sim/receiver/fake_lora/`, `sim/transmitter/fake_accelerometer/` and `sim/transmitter/fake_lora/` is a complete AVR-CMake project (`main.c` + `CMakeLists.txt`, one image per directory). They are bench/Proteus builds and duplicate coordinator logic on purpose; they are not part of the modular build groups above.

## Provenance

`real/Common/`, `real/Receiver/` and `real/Transmitter/` are the in-tree production sources maintained directly in this vault.

The five standalone projects were developed as separate Git repositories and were merged into this vault with their full commit history intact (`git log --follow -- <path>` still works):

| Path in vault | Original repo | Imported as |
|:--------------|:--------------|:------------|
| `real/ReceiverDevice/` | `landslideReciver_device` | merge commit `7a564db` |
| `real/TransmitterDevice/` | `landslideTransmitter_device` | merge commit `43c6fe9` |
| `sim/receiver/fake_lora/` | `receiver/fake_lora` | merge commit `c73505c` |
| `sim/transmitter/fake_accelerometer/` | `transmitter/fake_accelerometer` | merge commit `5571ba2` |
| `sim/transmitter/fake_lora/` | `transmitter/fake_lora` | merge commit `d9ada6a` |

The nested repositories' original branch refs are preserved under `refs/remotes/nested/`.

## See also

- [[Code Hub]] — decisions, module APIs, chip hierarchy
- [[realDevicePins]] — datasheet pinouts
- [[Firmware Hub]] — data formats and field behaviour
- [[Transmitter Simulation]] · [[Receiver Simulation]] — Proteus setup
