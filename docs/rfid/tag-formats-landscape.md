# Filament Tag Formats — Landscape

**Source:** SimplyPrint, "All about NFC/RFID for filament spools (Bambu, OpenPrintTag, Creality, Qidi, Anycubic & more)"
https://help.simplyprint.io/en/article/all-about-nfc-rfid-for-filament-spools-bambu-openprinttag-creality-qidi-anycubic-more-19luyni/
(page says "Updated on: 27/10/2026"; read 2026-11-09). Also read: their
[Creality CFS article](https://help.simplyprint.io/en/article/the-creality-material-standard-nfcrfid-for-the-creality-cfs-1crrofa/)
and [OpenPrintTag article](https://help.simplyprint.io/en/article/openprinttag-nfc-standard-in-simplyprint-1hjcz8/) (both 26/06/2026).

This is a vendor help page, not a spec. It lists chip types and app support. It gives **no byte layouts,
keys or offsets** for any format. For the CFS layout we use, `cfs-tag-write-design.md` (measured on real
tags) is the authority.

## Which formats this device can touch

The PN532 does ISO 14443A/B and FeliCa only (NXP datasheet, verified 2026-11-02). So:

| Format | Chip (per SimplyPrint) | PN532 can talk to the chip? | Notes |
|---|---|---|---|
| **Creality CFS** (K2, HI series) | MIFARE Classic 1K only. NTAG / Ultralight do not work | **Yes, this project** | AES-encrypted payload. See below |
| Qidi Box | MIFARE Classic 1K | Yes (chip level) | Few colors and material types, no free text. "3 fields", not named. https://wiki.qidi3d.com/en/QIDIBOX/RFID |
| Bambu Lab AMS | MIFARE Classic | Yes (chip level) | SimplyPrint can't write them. See the TODO Milestone 6 note on tag type |
| Anycubic ACE | NTAG213/215/216, MIFARE Ultralight (incl. Ultralight C) | Yes | No official docs |
| ELEGOO Canvas | NTAG213/215/216 | Yes | Plain NTAG, no encryption, no password. Brand always reads as "ELEGOO" |
| OpenSpool | NTAG215/216, NDEF | Yes | https://openspool.io |
| OpenTag3D | NTAG213/215/216 | Yes | https://opentag3d.info, see `opentag3d-spec.md` |
| TigerTag | NTAG213/215/216 | Yes | GPL-3.0, all data on the tag. https://tigertag.io |
| **OpenPrintTag** (Prusa) | ICODE SLIX2 / SLIX, ISO 15693 (NFC-V) | **No** | Decision 2026-11-02: no device-side support. See `openprinttag-spec.md` |

"Yes (chip level)" means the PN532 can read the chip. It does not mean we know the payload format or keys.

Tag capacities as SimplyPrint lists them: NTAG213 144 B, NTAG215 504 B, NTAG216 888 B, MIFARE Classic 1K
752 B, Ultralight C 144 B, ICODE SLIX 112 B, ICODE SLIX2 312 B (their OpenPrintTag page says 316 B;
the two pages disagree).

## Creality CFS: what SimplyPrint adds

Checked against what we measured:

- **MIFARE Classic 1K only, AES-encrypted.** Matches `cfs-tag-write-design.md` (sector 1, AES-128-ECB).
- **Fields:** material, full hex color, weight, serial, batch/date codes, vendor code. Same set as our
  34-char payload. They give no offsets.
- **Weight is stored as the original spool size in fixed steps: 250, 500, 600, 750, 1000 g.** An 800 g
  spool is written as 750 g. The tag never holds remaining weight.
  - **Confirmed in DnG-Crafts/K2-RFID v57** (`Arduino/ESP32/Spool_ID/Spool_ID.ino`, `GetMaterialLength()`):
    the length field only ever takes 5 values: `0330` 1 kg, `0247` 750 g, `0198` 600 g, `0165` 500 g,
    `0082` 250 g. Anything else defaults to `0330`. Our `330 * g / 1000` (integer math) gives the same
    5 values at those weights, so the formula is right.
  - **Fixed 2026-11-09** (`include/cfs_weight.h`). The weight slider is now 4 toggle buttons:
    250 g / 500 g / 750 g / 1 kg. 600 g is dropped from the UI but still decodes when read. 2 kg is
    left out on purpose: it would encode as `0660`, which is untested on the K2. Revisit when Prusa
    spools arrive. The inventory write path now writes the original weight instead of the remaining one.
    Reading `0247` now gives 750 g (the linear formula gave 748).
- **Material list depends on firmware:** about 36 types on some units, 56+ on others. If a tag isn't
  recognised, a "Generic"/"Hyper" variant of the same family is the safe fallback. Families every
  firmware knows: PLA (Hyper/Generic, Matte, Silk, High-Speed), PETG (+ PETG-CF), ABS (+ ABS-GF), ASA,
  TPU, PA, PC, PVA.
- **The CFS does not report a usable tag serial** for factory or untagged spools. SimplyPrint puts its
  own spool ID in tags it writes, and the CFS reports it back during sync. They don't say which field
  holds it; the 6-digit serial at 28–33 is the likely spot (inference). DnG does the same with the
  Spoolman spool ID. **ADR-001 chose otherwise:** the tag's hardware UID is the link, and no ID is
  encoded on the tag.
- **Phones:** iOS can't do MIFARE Classic at all, and Web NFC can't either. Android or a USB reader
  (ACR122U-class) works.
- Their "community docs" link, `github.com/Creality-Laser/K2-RFID`, is **broken (404)** as of
  2026-11-09. The `Creality-Laser` org is an unrelated account with 2 laser repos. The real community
  reference is **https://github.com/DnG-Crafts/K2-RFID**, already our reference implementation
  (`cfs-tag-write-design.md` §1). No commits since v57 (2026-04-13). Creality has published nothing official.

## OpenPrintTag: what SimplyPrint adds

- Desktop: **only the ACR1552U** reads ISO 15693. Other ACS readers (e.g. ACR122U) don't. This fits the
  PN532 finding: ISO 15693 needs a different reader.
- iOS and Android can both read and write it.
- Payload is CBOR. Fields: material class/type enum, brand, name, primary + up to 5 secondary colors,
  temperature ranges, nominal/remaining weight and length, diameter, density, dates, material tags,
  spool dimensions, GTIN, country of origin.
