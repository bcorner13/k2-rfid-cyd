# K2-RFID-CYD 🧵

A standalone, touchscreen-based RFID programmer for the Creality K2 Plus & CFS (Creality Filament System).

## 🚀 Features

* **Standalone operation** — No phone or PC required; program CFS tags from the touch screen.
* **Filament library** — Pick from material database (brand, type, color).
* **Tag read/write** — READ and WRITE buttons; MIFARE Classic 1K tags; CFS payload format (see `docs/rfid/creality-k2plus-rfid-spec.md`).
* **3-region layout** — Left: color block (tap to pick color); right: brand, type, volume; bottom: Read, Write, Library, Settings with status feedback.

## 🛠 Hardware (current target)

| Component | Details |
|-----------|---------|
| **Board** | Waveshare ESP32-S3 Touch LCD 4.3" (**4.3C** variant) |
| **Display** | 800×480 RGB LCD, LovyanGFX; touch via GT911 (I2C). Backlight/CTP reset via CH422G (U10). |
| **RFID** | PN532 (13.56 MHz); MIFARE Classic 1K tags. |
| **Docs** | [docs/board-variant-4.3C.md](docs/board-variant-4.3C.md) — pinout, schematic, CH422G init. |

## 📁 Documentation

| Doc | Description |
|-----|-------------|
| [docs/board-variant-4.3C.md](docs/board-variant-4.3C.md) | 4.3C board verification, schematic, LovyanGFX/CH422G notes. |
| [docs/CHECKPOINT.md](docs/CHECKPOINT.md) | Checkpoint state, build status, model summary, recent changes. |
| [docs/CODE-OVERVIEW.md](docs/CODE-OVERVIEW.md) | Module map and data flow (embedded + markdown overview). |
| [docs/rfid/creality-k2plus-rfid-spec.md](docs/rfid/creality-k2plus-rfid-spec.md) | CFS tag layout and sector usage. |

In-code documentation: see `@file` / `@brief` blocks in `include/filament_db.h`, `include/filament_profile.h`, `include/spool_data.h`, `include/rfid_driver.h`, `include/ui/ui_manager.h`, and `src/main.cpp`.

## 🔧 Build & upload

```bash
pio run
pio run -t upload
```

Default env: `waveshare_s3_43` (PlatformIO).

## 📜 Credits & acknowledgments

* **DnG-Crafts (K2-RFID)** — Initial decoding of Creality RFID hex structures and data formats.
* **OpenSpool** — CFS material database and vendor ID mappings.
* **LovyanGFX & LVGL** — Graphics and UI.

## ⚖️ License

MIT. See `LICENSE` for details.
