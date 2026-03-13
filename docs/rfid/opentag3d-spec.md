# OpenTag3D — Open Source RFID Standard for 3D Printer Filament

**Initiator:** Polar Filament / Bambu Research Group → OpenTag3D Consortium
**Licence:** Open source
**Status:** Finalization phase; adopted by Polar Filament, American Filament, Numakers, 3D Fuel, Ecogenesis Biopolymers
**Spec:** https://opentag3d.info/spec
**GitHub:** https://github.com/queengooborg/OpenTag3D
**Website:** https://opentag3d.info

---

## Overview

OpenTag3D is a community-driven RFID tag standard for 3D printer filament spools. Originally called "Open 3D-RFID," it was drafted by Polar Filament inside the Bambu Research Group (which reverse-engineered Bambu Lab's proprietary encrypted RFID tags). When it matured, it moved to its own repository under the OpenTag3D Consortium.

Key design goals:
- **No encryption** — fully open memory map, no vendor lock-in
- **Cheap hardware** — uses NTAG213/215/216 (off-the-shelf, ~$0.10–0.50/tag)
- **Smartphone readable** — standard NFC Type 2 / NDEF, readable by any NFC phone
- **PN532 compatible** — reads/writes with standard RFID modules over SPI (same hardware already in this project)

---

## Hardware

| Tag | Capacity | Usable | Feature Set |
|-----|----------|--------|-------------|
| NTAG213 | 144 bytes | 111 bytes | Core only |
| SLIX2 | 320 bytes | 287 bytes | Core + Extended |
| NTAG215 | 504 bytes | 471 bytes | Core + Extended |
| NTAG216 | 888 bytes | 835 bytes | Core + Extended |

- **Protocol:** ISO/IEC 14443 Type A, NDEF Type 2
- **Minimum:** 144 bytes writable capacity
- **Encoding:** NDEF record, MIME type `application/opentag3d`

> **Contrast with Creality CFS:** Both use ISO 14443-A at 13.56 MHz and are PN532-compatible. The difference is tag IC (NTAG21x vs MIFARE Classic 1K) and format (open NDEF vs encrypted proprietary sectors). The PN532 handles NTAG21x with `mifareultralight_ReadPage()` / `ntag2xx_WritePage()`.

---

## Memory Map

### Core Fields (0x00–0x6F, all 144-byte NTAG213 tags)

| Field | Offset | Length | Type | Notes |
|-------|--------|--------|------|-------|
| Tag Version | 0x00 | 2 bytes | uint (BE) | e.g. 1000 = v1.000 |
| Base Material | 0x02 | 5 bytes | UTF-8 | `PLA`, `PETG`, `TPU`, `ABS`… |
| Manufacturer | 0x1B | 16 bytes | UTF-8 | Brand name |
| Color 1 RGBA | 0x4B | 4 bytes | byte×4 | sRGB + alpha |
| Target Diameter | 0x5C | 2 bytes | uint (BE) | Micrometres (1750 = 1.75 mm) |
| Print Temperature | 0x60 | 1 byte | uint | °C ÷ 5 (42 = 210 °C) |
| Bed Temperature | 0x61 | 1 byte | uint | °C ÷ 5 |
| Density | 0x62 | 2 bytes | uint (BE) | µg/cm³ (1240 = 1.240 g/cm³) |

### Extended Fields (0x70–0xBA, SLIX2 / NTAG215 / NTAG216 only)

| Field | Offset | Length | Type | Notes |
|-------|--------|--------|------|-------|
| Online Data URL | 0x70 | 32 bytes | ASCII | Optional web lookup |
| Serial Number | 0x90 | 16 bytes | UTF-8 | |
| Manufacture Date | 0xA0 | 4 bytes | Date | |
| MFI Temperature | 0xA8 | 1 byte | uint | |
| Measured Weight | 0xAE | 2 bytes | uint | |
| Max Print Temp | 0xB5 | 1 byte | uint | °C ÷ 5 |

---

## Encoding Rules

- **Strings:** UTF-8
- **Integers:** Unsigned, big-endian
- **Temperatures:** Celsius value ÷ 5 (fits in 1 byte, range 0–255 → 0–1275 °C)
- **No encryption** required for compliance

---

## Physical Placement

- Tag centre: 56.0 mm from spool centre
- Max depth from outer surface: 4.0 mm
- Two tags recommended, on opposite spool ends (redundancy)

---

## Web API (Optional)

Tags can include a URL at 0x70. The endpoint returns JSON with `opentag_version`, `price`, and `product_url` keyed by country/region ISO code.

---

## Relevance to This Project

NTAG21x tags use ISO 14443-A (same RF layer as MIFARE Classic), so the PN532 hardware already present can read/write them without modification. Key differences from Creality CFS:

| | Creality CFS | OpenTag3D |
|---|---|---|
| Tag IC | MIFARE Classic 1K | NTAG213/215/216 |
| Auth | Key A (AES-derived) | None |
| Format | Fixed ASCII payload | NDEF (MIME typed) |
| Encryption | AES-128-ECB | None |
| PN532 driver call | `mifareclassic_*` | `mifareultralight_*` / `ntag2xx_*` |

Adding OpenTag3D read support to this programmer would mean:
1. Detecting NTAG21x vs MIFARE Classic on `readPassiveTargetID()` (ATQA/SAK bytes differ)
2. Reading NDEF payload with `mifareultralight_ReadPage()` in 4-byte pages
3. Parsing `application/opentag3d` MIME record and populating `SpoolData`
