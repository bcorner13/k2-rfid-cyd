# CFS Tag Write — Design

**Status:** Draft for review (2026-10-28)
**Branch:** `feat/cfs-tag-write`
**Replaces:** the `RFIDDriver::writeCFSTag()` stub (unimplemented since `cfbe802`)

---

## 1. Ground truth

Measured on hardware (MaTouch V3.1 + PN532), 2026-10-28:

| Tag | UID | Sector 1 key | Blocks 4–6 |
|---|---|---|---|
| Creality Hyper PLA spool | `1B:D9:8E:FB` | derived Key A works | AES-encrypted; decrypts to `2C5250276A210100100A29890165000001000000` + `05 00…` |
| Blank MIFARE Classic 1K | `61:E0:CF:E4` | derived fails, `FF×6` works | all `00` |

Reference implementation: `bcorner13/K2-RFID_newer` (fork of `donduce/K2-RFID_newer`),
`src/CFSTag.cpp` → `CFSTag::writeSpoolData()`.

What the K2 actually uses is **sector 1 only**:

- Blocks 4–6: 48 bytes of ASCII spool data, AES-128-ECB encrypted with `D_KEY`
  (`RFIDDriver::encryptBlock()` already exists and is the inverse of the working `decryptBlock()`).
- Block 7 (sector trailer): Key A = Key B = the key derived from the UID (`generateKeyA()`, already verified on a real spool).

> `creality-k2plus-rfid-spec.md` (the "K2PF" magic and binary sectors 1–15) does **not** match real tags.
> Do not use it for the CFS write path.

## 2. Payload

These fields are already produced by `SpoolData::_generateSpooldataString()`:

| Pos | Len | Field | Value written |
|---|---|---|---|
| 0–4 | 5 | Date code | `AB124` (default; same as reference) |
| 5–8 | 4 | Vendor | `0276` (Creality) |
| 9–10 | 2 | Batch | `A2` |
| 11 | 1 | Separator | `1` |
| 12–16 | 5 | **Material ID** | **`FilamentProfile.id`** (e.g. `01001`) — see bug B below |
| 17 | 1 | Color prefix | `0` |
| 18–23 | 6 | Color RGB hex | from profile / picker |
| 24–27 | 4 | Length | `330 * weight_g / 1000`, zero-padded |
| 28–33 | 6 | Serial | random 6 digits |
| 34–47 | 14 | Padding | ASCII `0` (reference behaviour) |

The real Creality tag has `000000` at 34–39, then `05 00 …`. The reference pads with ASCII `0`.
The reference project's users report the K2 accepts its tags. That is a claim we have not verified ourselves (open question 1).

## 3. Write sequence

```
1. Select tag: readPassiveTargetID. Require a 4-byte UID (MIFARE Classic 1K); refuse others.
2. Determine key state:
     a. auth block 7 with derived key  → state = PROVISIONED (re-write)
     b. else re-select tag; auth block 7 with FF×6 → state = BLANK
     c. else → fail "Unknown tag keys" (no writes performed)
3. plaintext[48] = payload, '0'-padded; encrypt as 3 × 16-byte ECB blocks
4. write blocks 4, 5, 6
5. if BLANK:
     read block 7 → trailer
     SAFETY CHECK: trailer[6..8] must equal FF 07 80 (factory transport access bits);
                   otherwise abort — never write a trailer with unknown access conditions
     trailer[0..5]   = derived key  (Key A)
     trailer[10..15] = derived key  (Key B)
     trailer[6..9]   unchanged      (access bits + GPB)
     write block 7
6. verify: re-select, auth block 4 with derived key, read 4–6, decrypt, compare to plaintext
7. return a result enum, not a bool:
     OK | NO_TAG | UNSUPPORTED_TAG | UNKNOWN_KEYS | WRITE_FAILED | TRAILER_UNSAFE | TRAILER_FAILED | VERIFY_FAILED
```

**Why this can't brick a tag:**
- Access bits are never changed. Only the two key fields change, and only when the factory access bits (`FF 07 80`, under which Key A may rewrite the keys) are confirmed.
- **Interrupted after step 4:** the keys are still `FF×6`. The next attempt takes path 2b and finishes the job.
- **Interrupted during step 5:** a MIFARE block write is atomic, so the trailer is either the old one or the new one. Either path 2a or 2b recovers.

## 4. Explicitly out of scope

- Sectors 2–15. The existing `writeTag()` path (the project's v1/v2 format with mirrors, CRC and extended sectors) writes a `TagFormatBlock` with the "K2PF" magic to **block 4**. That would destroy the CFS payload. It must not run against a CFS tag. Moving the project's extension data to other sectors is a separate decision.
- Changing the CFS write-back behaviour of the K2 printer itself.

## 5. Bugs fixed in this branch

| ID | Bug | Fix |
|---|---|---|
| A | `writeCFSTag()` is a stub that returns `true` without writing | Implement §3 |
| B | `SpoolData(FilamentProfile)` writes `meterialType` (`"PLA  "`) into the material-ID field | Use `profile.id` (`"01001"`) |
| C | After a failed auto-read of a blank tag, the next poll reports "Tag removed" while the tag is still on the reader. Write's presence check then fails → long beep | Reproduce with logging first, then re-select or release the PN532 target after a failed read. The reference resets and wakes the PN532 before retrying |
| D | Failure locks the UI for 10 s (`enterError("Tag write failed", 10000)`) with no cause shown | Show the specific result enum on `labelWriteStatus`; keep tap-to-dismiss |
| E | A blank tag shows "Reading..." forever | When sector 1 is all zeros under `FF×6`, report "Blank tag — ready to write" |

## 6. Test plan

1. Spare blank tag → Write → expect `OK`. The serial log shows the BLANK path and the trailer check passing.
2. Lift and replace the tag → auto-read decodes the written payload: material, color, weight.
3. Write again to the same tag → PROVISIONED path, `OK`.
4. Cut power mid-write if practical → the next write recovers (§3).
5. Load the tag in the K2 Plus CFS → the printer shows the correct material, color and weight. **This is the acceptance test.**

## 7. Open questions

1. Padding: ASCII `0` (reference) vs. the `000000` + `05 00…` seen on a real tag. Start with the reference. If the K2 rejects the tag, try matching the real tag byte-for-byte.
2. Date, batch and serial defaults: keep `AB124` / `A2` / random serial, or derive the date from the RTC?
3. Should Write require a confirmation before overwriting a tag that already decodes as a valid CFS spool?
