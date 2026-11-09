# Spoolman Integration — Design Draft

**Status:** Draft (2026-10-29). Implements [ADR-001](ADR-001-inventory-in-spoolman.md).
**Spoolman version targeted:** v0.27.0 or later (needed for tag-UID linking). The live server runs v0.27.1 (from its startup log, 2026-11-05). The lookup below was re-checked against the v0.27.1 source (`spoolman/api/v1/{models,spool,tag}.py`). Re-check if the server version differs.

## Spoolman data model (fields we use)

| Entity | Fields |
|---|---|
| Vendor | `id`, `name` |
| Filament | `id`, `name`, `vendor`, `material`, `color_hex`, `weight` (net g), `spool_weight`, `diameter`, `density`, `settings_extruder_temp`, `settings_bed_temp`, `external_id`, `extra{}` |
| Spool | `id`, `filament`, `initial_weight`, `remaining_weight`, `used_weight`, `remaining_length`, `location`, `lot_nr`, `comment`, `archived`, `extra{}`, `tags[]` |
| Tag | `uid` (normalized hex), `format` (`"creality"`), linked to one spool or filament |

`PUT /api/v1/spool/{id}/use` consumes weight or length. There are websockets for live updates (not needed in phase 1).

## Flows

### 1. Tag placed (auto-read): look up
```
read CFS tag (existing readCFSTag) -> UID + payload
GET /api/v1/spool?tag=<UID>&allow_archived=true
  200 + spool carrying this UID -> Spoolman line shows id, vendor + name, remaining_weight, location
  200 + []                      -> "Not in Spoolman"  (phase 2 adds "Add?", flow 2)
  200 + spool without this UID  -> "Spoolman error", never shown as the tag's spool
  unreachable or DNS failure    -> show tag data only (today's behavior), "Spoolman offline"
```
- `allow_archived=true`: without it, a tag on an archived spool looks unknown, and linking it would then hit a 409.
- The result has its own line on the main screen, because auto-read re-fires every 3 s and rewrites the status text. Only one lookup runs per placement: lift and replace the tag to look it up again.
- A tag linked to a filament rather than a spool shows as "Not in Spoolman" here. Linking it in phase 2 reports the holder through the 409.
- **Alternative for later:** `POST /api/v1/tag/scan` also resolves a UID (spool or filament) and broadcasts the scan to Spoolman's websocket clients. It is a POST with a side effect, so phase 1 doesn't use it. Consider it for phase 2.

### 2. Add unknown tag to inventory
```
filament = find by CFS material id (external_id/extra), else by vendor+name, else create from the DB profile
POST /api/v1/spool  {filament_id, initial_weight: <from tag length>, location?: "K2 CFS"}
POST /api/v1/spool/{id}/tag  {uid, format: "creality"}
   409 -> UID already linked to another spool: offer "Move tag here?"
```

### 3. Tag written (armed write, after OK)
- If the UID is already linked: offer to update that spool's filament to match what was just written.
- If not linked: offer flow 2 with the written filament preselected.

### 4. Inventory screen
`GET /api/v1/spool?archived=false` gives a scrollable list (name, color swatch, remaining g, location). Tapping a spool shows details, with "Write tag for this spool" running the armed flow pre-filled from the spool's filament. Editing stays in Spoolman's web UI.

### 5. Printer remaining amount → Spoolman (phase 2)
Measured 2026-11-09 via Moonraker (read-only GETs, printer at `192.0.2.107`). There are two sources.

**a. `box.Tn.remain_len[]`: the CFS's own estimate.** The load sequence feeds until the extruder sensor
triggers, and the box works out what's left from wheel travel against spool rotation. `box.cfg` macro
`BOX_INFO_REFRESH` runs `BOX_GET_RFID` then `BOX_GET_REMAIN_LEN` per slot. For non-RFID spools the box
returns 255 and klippy logs `GET_REMAIN_LEN value is 255 ... likely means the data is unavailable, skip
updating`, so `remain_len` stays `"0"`. Every CFS slot held non-RFID spools for the whole 2026-11-04..09
log window, so **what it reports for a tagged spool is not yet observed.**
Test: load a genuine or device-written tagged spool into a CFS slot, let it run the load sequence, then
`GET /printer/objects/query?box`. Units and whether it updates during a print are unknown.

**b. `box.Tn.measuring_wheel`: a per-box running odometer.** Float, about 1 unit per mm, negative =
feeding. It sits in the `box` status and klippy logs it every ~3 s (`statistics_ext ... measuring_wheel = N`).
Check: during the 2026-11-06 23:22 → 11-07 09:07 PLA job it moved −119,000 → −235,400 = 116,400.
Moonraker history logged `filament_used` 118,730 mm, so 98%. It is **per box, not per slot**, so the
delta has to be credited to whichever slot was feeding. Unload retracts show up as positive steps (+1,339 at
the end of that job).

**Plan:** pick (a) once it is verified for tagged spools. It's the printer's own number and survives the
device being off. Otherwise use (b): record the wheel at job start and end (Moonraker `print_stats`
state changes), credit the delta to the active slot's spool, and send `PUT /api/v1/spool/{id}/use`
with `use_length` in mm. Spoolman converts length to weight with the filament's density and diameter.
Slot → spool comes from the tag UID read at the slot (needs the CFS to expose a UID per slot, which is
not yet seen) or from a manual assignment made on the device.
- Moonraker's built-in `[spoolman]` component isn't loaded (`/server/spoolman/status` → 404). It tracks
  extruder position for one "active spool" and knows nothing about CFS slots, so it would need
  `SET_ACTIVE_SPOOL` on every slot change. Enabling it means editing the printer's `moonraker.conf`:
  printer-side change, ask first.
- The tag can't carry this. Its length field is the original spool size only (one of five steps,
  `include/cfs_weight.h`), and the K2 never writes tags.

## Device changes

- Config: `spoolman_url` (default `http://spoolman.lan:7912`, blank = off), set via the WiFi portal. `https://` is recognized but reports "not supported yet": it needs a TLS client and certificate handling, as a separate step, and is never silently downgraded.
- **DNS:** this LAN's first DNS server is the Pi that hosts Spoolman (`192.0.2.97`), so `spoolman.lan` stops resolving when the Pi is down. Set an IP in the portal if that becomes a problem.
- `SpoolmanClient` (`src/spoolman_client.cpp`): HTTP runs on its own FreeRTOS task with short timeouts, because DNS can block well past the connect timeout. It talks to the main loop through single-slot queues and never touches LVGL or RFID. Parsing is in `include/spoolman_parse.h`, tested on the host with `pio test -e native`.
- **Unverified for phase 2:** v0.27.1 logs "Trusting writes and websockets from this instance's own origin only". Confirm a device POST with no `Origin` header is accepted before building add/link.
- Replace `InventoryManager` usage in `ui_manager.cpp` (Scan Tag, Spool Detail, Custom Entry). Remove `InventoryManager` once the replacement works.
- Spool Detail "Write Tag" moves to the armed write flow at the same time.

## Phasing

1. Config + `SpoolmanClient` + lookup on auto-read (flow 1). Read-only, so it can't damage inventory data.
2. Add / link (flow 2) and post-write offer (flow 3).
3. Inventory screen (flow 4) and removal of `InventoryManager`.
4. Printer remaining-amount sync (flow 5), after the K2 investigation.

## Verification

Each phase is tested against the real Spoolman on the HA Pi, using a disposable test spool and tag. Confirm with Spoolman's web UI, not only the device's view of it.
