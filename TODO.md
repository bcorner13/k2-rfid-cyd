# K2 RFID Tag Programmer - TODO

## 🎯 Current Focus
- Verification of CFS tag data reading and writing on real hardware.

## ✅ Completed (2026-03-10)
- [x] **Hardware Migration:** Successfully moved from Waveshare to Makerfabs MaTouch 4.3" V3.1.
- [x] **RFID Auto-Read:** Background task implemented to automatically detect and read tags.
- [x] **WiFi Stability:** Fixed reboots during Setup Portal by switching to non-blocking mode and increasing watchdog timeouts.
- [x] **Display Fix:** Eliminated screen shakes by moving LVGL buffer to Internal RAM and lowering PCLK to 12MHz.
- [x] **Persistent Config:** WiFi Printer IP now saved correctly from the portal to `config.json`.
- [x] **Splash Screen:** Added a 10-second review window with real-time WiFi connection status.

## 🚀 Priority Tasks
- [ ] **Data Mapping:** Verify all fields in `SpoolData` (length, batch, date code) match Creality K2 Plus standards.
- [ ] **Mirror Voting:** Implement the 3-way mirror sector comparison (Sectors 6, 7, 8) per FSD Section 7.8.
- [ ] **Battery/Voltage:** Map the analog pin for battery monitoring on MaTouch (if available) and update the UI overlay.
- [ ] **Sound:** Verify passive buzzer on GPIO19 and implement varied beep patterns for Success/Failure.

## 🛠 Features & UI
- [ ] **Inventory Sync:** Sync scanned spools with the local inventory database automatically.
- [ ] **Cloud Sync:** Propose/implement a sync mechanism with a central filament database.
- [ ] **Settings UI:** Add brightness slider and beep volume control (if applicable).
- [ ] **Sleep Mode:** Implement automatic screen timeout and wakeup on touch.

## 🐛 Known Issues
- [ ] **WiFi Portal Close:** Harmless `WebServer.cpp:638` error when portal closes while client is still polling.
- [ ] **MaTouch V3.1 Audio:** Audio path for the new onboard speaker connector needs verification (likely I2S).
