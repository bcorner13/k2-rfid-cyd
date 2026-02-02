# K2-RFID-CYD 🧵

A standalone, touchscreen-based RFID programmer for the Creality K2 Plus & CFS (Creality Filament System). Built for the **ESP32-2432S028** "Cheap Yellow Display."

## 🚀 Features
* **Standalone Operation:** No phone or PC required; program tags directly from the 2.8" touch screen.
* **Inventory Management:** Track remaining spool weight and "Cold Storage" stock.
* **Safe Erase:** Protocol-aware logic to reset tags without bricking sector trailers.
* **Visual/Audible Feedback:** Onboard RGB LED and Piezo support for success/error states.

## 🛠 Hardware
* **Display:** ESP32-2432S028 (2.8" TFT Touch)
* **Reader:** RC522 RFID Module (13.56MHz)
* **Tags:** Mifare Classic 1K (Wet Inlays or Stickers)

## 📜 Credits & Acknowledgments
This project builds upon the groundbreaking work of the Creality community:
* **DnG-Crafts (K2-RFID):** For the initial decoding of the Creality RFID hex structures and data formats.
* **OpenSpool Project:** For insights into the CFS material database and vendor ID mappings.
* **TFT_eSPI & LVGL:** For the graphics engines powering the native UI.

## ⚖️ License
Distributed under the MIT License. See `LICENSE` for more information.
