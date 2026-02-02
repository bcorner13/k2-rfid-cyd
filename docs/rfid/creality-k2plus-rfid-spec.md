# Creality K2 Plus RFID Tag Specification
## MIFARE Classic 1K — Formal Layout Specification

**Status:** Draft v1.0  
**Scope:** Creality K2 Plus filament RFID compatibility  
**Tag Type:** MIFARE Classic 1K only  
**Audience:** Firmware developers, reverse engineers, integrators

---

## 1. Purpose

This document defines the **exact RFID memory layout, encoding rules, and behavioral constraints** required for a MIFARE Classic 1K RFID tag to be **accepted and actively used** by the Creality K2 Plus printer and CFS system.

A tag conforming to this specification SHALL:
- Be recognized as a valid filament spool
- Display filament metadata
- Track filament consumption
- Participate in CFS runout and switchover logic

---

## 2. Physical Tag Characteristics

- **Tag Type:** MIFARE Classic 1K
- **Total Memory:** 1024 bytes
- **Sectors:** 16
- **Blocks per Sector:** 4
- **Bytes per Block:** 16
- **Sector Trailer:** Block 3 of each sector

---

## 3. Sector Classification

| Sector | Purpose | Mutability |
|------|--------|-----------|
| 0 | Manufacturer / UID | Immutable |
| 1 | Tag format & version | Immutable |
| 2 | Filament identity | Immutable |
| 3 | Material & color | Immutable |
| 4 | Vendor / product metadata | Immutable |
| 5 | Spool initialization | Write-once |
| 6 | Remaining filament (primary) | Mutable |
| 7 | Remaining filament (mirror A) | Mutable |
| 8 | Remaining filament (mirror B) | Mutable |
| 9 | Usage counters | Mutable |
| 10–14 | Reserved | MUST NOT be modified |
| 15 | Control / checksum | Mutable (controlled) |

Sector trailers SHALL NOT be modified except during controlled reinitialization.

---

## 4. Encoding Conventions

### 4.1 Numeric Encoding
- All multi-byte numeric values SHALL be **little-endian**
- Integers SHALL be fixed-width
- Floating point values MUST NOT be stored on-tag

### 4.2 String Encoding
- ASCII or UTF-8 subset
- Fixed-length fields
- Null-padded
- No length prefixes

### 4.3 Padding
- Unused bytes SHALL be zero-filled
- Garbage or uninitialized bytes MAY cause rejection

---

## 5. Sector-Level Layout (Normative)

### 5.1 Sector 0 — Manufacturer / UID (Immutable)

- Managed by tag manufacturer
- MUST NOT be modified
- Any write attempt SHALL be rejected by tooling

---

### 5.2 Sector 1 — Tag Format & Version (Immutable)

**Block 4 (Sector 1, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Magic constant (`0x4B325046` = "K2PF") |
| 0x04 | 1 | RFID format version |
| 0x05 | 1 | Printer compatibility mask |
| 0x06 | 2 | Reserved (0x0000) |
| 0x08 | 8 | Reserved (zero-filled) |

- Magic constant MUST match exactly
- Unknown format versions MAY be rejected by printer

---

### 5.3 Sector 2 — Filament Identity (Immutable)

**Block 8 (Sector 2, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Vendor product ID |
| 0x04 | 2 | Material type enum |
| 0x06 | 2 | Filament diameter (µm) |
| 0x08 | 8 | Reserved |

---

### 5.4 Sector 3 — Material & Color (Immutable)

**Block 12 (Sector 3, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Color RGB (0x00RRGGBB) |
| 0x04 | 12 | Color name (ASCII, null-padded) |

---

### 5.5 Sector 4 — Vendor Metadata (Immutable)

**Block 16 (Sector 4, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 12 | Vendor name |
| 0x0C | 4 | Vendor revision / batch |

---

### 5.6 Sector 5 — Spool Initialization (Write-Once)

**Block 20 (Sector 5, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Initial filament length (mm) |
| 0x04 | 4 | Initial filament weight (g) |
| 0x08 | 8 | Reserved |

Once written, this sector SHALL NOT be modified.

---

### 5.7 Sectors 6–8 — Remaining Filament (Mutable, Mirrored)

Each sector stores the same structure.

**Blocks 24 / 28 / 32**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Remaining filament length (mm) |
| 0x04 | 4 | Remaining filament weight (g) |
| 0x08 | 4 | Update counter |
| 0x0C | 4 | Reserved |

Printer MAY:
- Write to one or more mirrors
- Validate consistency across mirrors

Tooling MUST update all mirrors consistently.

---

### 5.8 Sector 9 — Usage Counters (Mutable)

**Block 36 (Sector 9, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | Total consumed length (mm) |
| 0x04 | 4 | Total consumed weight (g) |
| 0x08 | 8 | Reserved |

---

### 5.9 Sector 15 — Control & Checksum

**Block 60 (Sector 15, Block 0)**

| Offset | Size | Field |
|------|------|------|
| 0x00 | 4 | CRC32 of sectors 1–9 |
| 0x04 | 12 | Reserved |

CRC MUST be recomputed on every write affecting sectors 1–9.

---

## 6. Authentication & Access Control

- Key A SHALL be used for data access
- Key B SHALL be valid but MAY be unused
- Access bits MUST allow:
    - Read/write for data blocks
    - Write-protected trailers

Incorrect access configuration MAY result in silent printer rejection.

---

## 7. Write Semantics (Transactional)

Every write operation SHALL:

1. Authenticate sector
2. Read existing data
3. Validate format & version
4. Validate battery voltage
5. Write full block(s)
6. Read back
7. Byte-compare
8. Update CRC
9. Confirm success

Partial writes are forbidden.

---

## 8. Immutable vs Mutable Enforcement

| Region | Rule |
|----|----|
| Sectors 1–4 | MUST NOT change after initialization |
| Sector 5 | Write-once |
| Sectors 6–9 | Mutable |
| Sector trailers | MUST NOT change |
| Sector 0 | MUST NOT change |

Tooling SHALL enforce these rules.

---

## 9. Compliance Checklist

A compliant tag MUST:

- Authenticate all required sectors
- Match magic constant
- Use known format version
- Maintain valid CRC
- Permit printer write-back
- Survive power cycles

---

## 10. Versioning

- This document defines **RFID Format Version 1**
- Future versions MUST:
    - Preserve backward compatibility
    - Use a new format version byte
    - Update CRC coverage rules if needed

---
# 11. Repository Recommendation

Suggested path:
```
docs/
└── rfid/
└── creality-k2plus-rfid-spec.md
```