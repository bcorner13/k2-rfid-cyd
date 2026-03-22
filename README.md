# K2 RFID Tag Programmer

Standalone RFID programmer for Creality K2 Plus CFS (Creality Filament System).

## 🚀 Status: Milestone 1 Complete (2026-03-20)
All 9 hardware-observed bugs fixed on the **Makerfabs MaTouch ESP32-S3 4.3" V3.1**. RFID reads real Creality spools reliably, audio feedback is active, splash screen shows live WiFi status, and the UI has no lockout paths.

## 🛠 Hardware
- **MCU:** ESP32-S3 (16MB Flash, 8MB PSRAM)
- **Display:** 4.3" 800x480 IPS (Parallel RGB)
- **Touch:** GT911 Capacitive
- **RFID:** PN532 via Mabee I2C Port
- **Sound:** Passive Buzzer (GPIO19) / I2S (V3.1 Speaker)

## ✨ Key Features
- **Auto-Read:** Background task detects and reads tags automatically when waving near the reader.
- **WiFi Setup:** Non-blocking configuration portal with dedicated setup screen.
- **Stable UI:** Zero screen flicker or shakes during touch/I2C activity.
- **Filament DB:** Local cache of filament profiles for quick tag writing.

## ⚡ Quick Start
1.  **Build & Flash:**
    ```bash
    pio run -t upload
    pio run -t uploadfs
    ```
2.  **Initial Setup:**
    - On boot, review status for 10 seconds on the splash screen.
    - Go to **Settings -> Setup WiFi** to connect to your network and enter your Printer IP.
3.  **Read Tag:** Simply wave a tag near the PN532 reader while on the dashboard.

## 📄 Documentation
- [CHECKPOINT.md](docs/CHECKPOINT.md) - Migration history and technical details.
- [TODO.md](TODO.md) - Current development roadmap.
- [CODE-OVERVIEW.md](docs/CODE-OVERVIEW.md) - Architectural deep-dive.
