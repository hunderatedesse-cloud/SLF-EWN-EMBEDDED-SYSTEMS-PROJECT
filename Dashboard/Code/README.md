# SLF-EWN — Subsurface Early Warning

Desktop Qt 6 console for a subsurface sensor network. Field nodes report soil moisture and 3-axis tilt. A USB-UART **receiver** stores those samples; this dashboard pulls them on command, classifies each node as **SAFE**, **WARNING**, or **CRITICAL**, journals alerts, and keeps history locally.

The UI is Inter + JetBrains Mono, bundled with the app.

## What it does

- Live node cards with moisture, tilt, sparklines, and status
- Per-node detail: 24 h / 7 d / 30 d charts, acknowledge, zone label, optional custom alarm limits
- Alert log with filters, acknowledge / clear, and CSV export
- Geographic **Map View**: OpenStreetMap or satellite tiles, place polygons, click-to-place nodes
- Automatic site assignment from lat / lon / altitude boxes
- USB-UART at **115200 8N1** with auto-connect, receiver dump commands, link diagnostics, and a simulation mode
- Local SQLite store for telemetry, alerts, node profiles, places, and operator accounts
- Admin vs guest sessions (guests are read-only)

## Requirements

- Qt 6: Core, Gui, Quick, QuickControls2, Qml, SerialPort, QuickDialogs2, Sql, **Network**
- C++17 compiler (MSVC, GCC, or Clang)
- CMake 3.16+ **or** qmake
- Network access for map tiles (cached under the app cache directory)

On Windows, install [Qt 6](https://www.qt.io/download-qt-installer) with the **Serial Port**, **SQL** (SQLite), and **Network** modules.

## Build

### CMake

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=<path-to-Qt6>
cmake --build build --config Release
```

The binary is named `SLF-EWN` (`SLF-EWN.exe` on Windows).

### qmake

```bash
qmake SlfEwnDashboard.pro
make          # nmake / mingw32-make on Windows
```

### Protocol smoke test

`tools/protocol_smoke.cpp` checks 12-byte LoRa frames, 16-byte dump records, STATUS / FULL_DUMP headers, chunked reassembly, and CRC rejection. Build it against `src/core` (Crc8, PacketParser, Types) if you change the wire format.

## First run

The login screen is the session gate.

| Account | Password | Role |
|---------|----------|------|
| `admin` | `admin`  | Full control (thresholds, serial, places, acknowledge, users) |
| `guest` | `guest`  | Live view only |

You can also continue as guest without a password. Change the admin password after first login; Settings → **Access** warns while the default is still in use.

Five failed logins lock the form for 30 seconds.

## Using the console

1. Sign in.
2. The dashboard auto-opens the preferred USB-UART port (simulation is **off** by default). Pick another port from the serial bar if needed, or enable simulation in Settings → **Link**.
3. On a live receiver, send **Refresh** (FULL_DUMP `0x44`). Nodes appear as records decode. Later refreshes use INCREMENTAL (`0x4E` + last RID) when a RID is known.
4. Click a node card for the detail panel.
5. **Alert Log** lists warning / critical / hardware-interrupt events. Admins can acknowledge or export CSV.
6. **Map View** pans/zooms OSM or satellite imagery, draws place coverage, and lets you drop or edit node coordinates.
7. **Settings**
   - **Thresholds** — default moisture and tilt warning/critical levels (38 % / 58 %, 8° / 16°) plus a live probe preview
   - **Places** — named sites with start/end lat, lon, optional altitude; nodes inside the box are assigned automatically
   - **Nodes** — labels, geo, per-node alarm overrides
   - **Link** — port, baud, Refresh / Clear log, RX stats, store path, simulation
   - **Access** — users, password change, audit trail (admin)

Hardware emergency is bit 7 of the status byte (`0x80`). That raises an **Emergency-Interrupt** regardless of moisture/tilt.

## Serial protocol (receiver v1.0)

Link: **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.** DTR and RTS are asserted so USB-UART bridges (Arduino-style boards included) start transmitting.

The receiver **does not stream unsolicited**. The dashboard sends one command, waits for a complete reply, then may send the next.

### Host → receiver (commands)

| Byte | Name | Payload |
|------|------|---------|
| `0x53` (`S`) | STATUS | none |
| `0x44` (`D`) | FULL_DUMP | none |
| `0x4E` (`N`) | INCREMENTAL | 4-byte big-endian last record ID |
| `0x43` (`C`) | CLEAR | none (erases EEPROM; next RID is 0) |

### Receiver → host (replies)

```
0xAA | cmd | uint32be length | payload
```

| cmd | Payload |
|-----|---------|
| STATUS | 8 bytes: `write_count` + `free_records` (both uint32be) |
| FULL_DUMP / INCREMENTAL | `N × 16` dump records |
| CLEAR | empty |
| `0xFF` ERROR | 1-byte code (`0x02` = timeout / bad payload) |

### Dump record (16 bytes, big-endian)

| Offset | Size | Field |
|--------|------|--------|
| 0 | u32 | Record ID |
| 4 | 12 B | LoRa node frame (below) |

### LoRa node frame (12 bytes, big-endian)

| Offset | Size | Field |
|--------|------|--------|
| 0 | u16 | Device ID (`NODE<n>` when id &lt; `0xA000`, else 4-digit hex) |
| 2 | u16 | Moisture raw (`0…10000` → `0.00…100.00 %`) |
| 4 | i16 | Tilt X (radio raw −1950…1950 → −180°…180°) |
| 6 | i16 | Tilt Y |
| 8 | i16 | Tilt Z |
| 10 | u8 | Hardware status (`0x80` = emergency) |
| 11 | u8 | CRC-8/SMBUS over bytes 0–10 (poly `0x07`, init `0x00`) |

Tilt magnitude used for alarms is the max of `|X|`, `|Y|`, `|Z|` in degrees.

Settings → **Link** shows RX bytes, decoded frames, discarded bytes, dump phase, and a short diagnosis if the feed is silent or mis-framed.

### Fallback: unsolicited text or raw 12-byte frames

When no command is in flight, the same port still accepts:

**ASCII / LoRa lines** (newline-terminated):

```
NODE104_DATA:X_12:Y_-3:Z_980:MOIST_42.5%
NODE104_DATA:X_12:Y_-3:Z_980:MOIST_42.5% : EMERGENCY
A104,42.5,1.2,-0.4,0.8
0xB201 3850 120 -40 980 0
```

- LoRa lines: `NODE<id>_DATA:X_<raw>:Y_<raw>:Z_<raw>:MOIST_<pct>%` with optional `: EMERGENCY`
- CSV/whitespace: field 1 device ID (hex, optional `0x`); field 2 moisture (`≤ 100` as percent, else raw); fields 3–5 tilt; field 6 optional hardware status
- Lines starting with `#` are ignored

**Raw 12-byte frames** are aligned by CRC. Bytes that fail CRC are discarded one at a time until a valid frame is found.

## Map View

- OSM tiles by default; satellite uses ArcGIS World Imagery
- Tiles are HTTP-cached (~96 MB) under the Qt cache location `…/map-tiles`
- Places are geographic boxes (two corners + optional altitude band). The smallest covering place wins
- Nodes without coordinates stay off the map until placed from Map View or Settings → **Nodes**

## Local store

SQLite file:

```
<AppData>/SLF-EWN/SLF-EWN/slfewn.db
```

On Windows that is typically `%APPDATA%\SLF-EWN\SLF-EWN\slfewn.db`. Samples are buffered and flushed every 4 seconds (or sooner if the queue fills). Passwords are stored as PBKDF2-HMAC-SHA256, not plaintext.

## Layout

```
src/
  core/          packet parse, CRC-8, shared types
  model/         serial, SQLite, simulation, list models
  controller/    app session, auth, node state machine
qml/
  pages/         login, dashboard, alerts, geo map, settings
  components/    cards, charts, chrome
  fonts/         Inter, JetBrains Mono
tools/
  protocol_smoke.cpp
```

`App` and `Auth` are QML context properties on `Slf::AppController` and `Slf::AuthController`.

## License

Private / project use unless otherwise specified.
