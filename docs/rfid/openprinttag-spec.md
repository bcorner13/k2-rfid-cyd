# OpenPrintTag — Open NFC Standard for 3D Printing

**Initiator:** Prusa Research
**Licence:** MIT
**Status:** Shipping on Prusament spools (Nov 2025)
**Spec:** https://specs.openprinttag.org
**GitHub:** https://github.com/prusa3d/OpenPrintTag
**Announcement:** https://blog.prusa3d.com/the-openprinttag-is-here-a-brand-new-nfc-tag-standard-for-smart-filament-is-now-shipped-with-a-new-redesigned-prusament-spool_123878/

---

## Overview

OpenPrintTag is Prusa Research's open-source NFC tag standard for smart filament spools. Unlike Creality's CFS tag (MIFARE Classic 1K, encrypted, proprietary), OpenPrintTag is:

- **Writable** by printers and smartphones — no vendor tooling required
- **Offline-first** — all data lives on the tag itself; no cloud dependency
- **Universal** — designed for filament, resin, and other 3D printing materials
- **Reusable** — empty spool? Peel the tag off and reprogram it for new filament

---

## Hardware

| Property | Value |
|----------|-------|
| Frequency | 13.56 MHz |
| Standard | ISO 15693 |
| Tag type | ISO 15693 / NFC Type 5 (industrial grade) |
| Orientation | 360° readable (circular tag layout) |
| Max spool weight | 2 kg |

> **Contrast with Creality CFS:** CFS uses ISO 14443-A (MIFARE Classic 1K) at 13.56 MHz — a different tag family to OpenPrintTag's ISO 15693. The PN532 supports both, but they use different commands.

---

## Data Format

All essential data is stored directly on the tag:

| Field | Notes |
|-------|-------|
| Material type | String |
| Color | RGB |
| Remaining filament | Live-updated length/weight |
| Print settings | Temperatures, retraction, etc. |
| Batch / traceability | Manufacturer batch data |
| URL | Link to rich product page (smartphone scan) |

Full byte-level memory map: https://specs.openprinttag.org

---

## Implementations

| Language | Repository |
|----------|-----------|
| Python | https://github.com/prusa3d/OpenPrintTag (utilities included) |
| C++ | https://github.com/Prusa3d/Prusa-Firmware-Buddy/tree/openprinttag/src/module/nfc/openprinttag |
| Flutter/Dart | https://github.com/OpenPrintTag/openprinttag-dart |
| JavaScript | Planned |

---

## Relevance to This Project

The PN532 on the MaTouch board supports ISO 15693 via the `inJumpForDEP` / `inListPassiveTarget` command path. Adding OpenPrintTag read support would allow this programmer to:

1. Detect whether a presented tag is a Creality CFS tag (ISO 14443-A) or an OpenPrintTag (ISO 15693)
2. Display material info from either format
3. Potentially write OpenPrintTag-formatted tags for use with Prusa printers

This would require the Adafruit PN532 library's low-level RF commands or a separate ISO 15693 driver, since the high-level `readPassiveTargetID()` call only handles ISO 14443-A.
