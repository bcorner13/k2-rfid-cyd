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

> **Contrast with Creality CFS:** CFS uses ISO 14443-A (MIFARE Classic 1K) at 13.56 MHz — a different tag family to OpenPrintTag's ISO 15693. **The PN532 does not support ISO 15693** (verified 2026-11-02 against the NXP PN532 datasheet: reader/writer modes are ISO 14443A/MIFARE, ISO 14443B and FeliCa only). Prusa's official tags are NXP ICODE SLIX2 (ISO 15693, ~316 usable bytes). Data is an NDEF record (`application/vnd.openprinttag`) holding CBOR maps; that layout is from a community implementation, so check it against the official repo.

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

The PN532 on the MaTouch board **cannot** read or write OpenPrintTag. An earlier version of this doc said it could via `inListPassiveTarget`; that was wrong (corrected 2026-11-02, see Hardware above). Options:

1. **Second reader for NFC-V:** an NXP PN7160 or ST ST25R3916 module (both handle ISO 14443A *and* ISO 15693, so one could eventually replace the PN532). Needs a new driver; the Adafruit PN532 library won't help.
2. **No device-side OpenPrintTag:** the Prusa printer reads and writes its own tags. The device only needs to see Prusa spools through Spoolman (ADR-001), not handle their tags.

Option 2 costs nothing and may be enough. Decide after the Core One L+ arrives and we see what it reports over its API (PrusaLink/Prusa Connect) about loaded spools.
