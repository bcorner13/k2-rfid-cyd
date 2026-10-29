# Spoolman Integration — Design Draft

**Status:** Draft (2026-10-29). Implements [ADR-001](ADR-001-inventory-in-spoolman.md).
**Spoolman version targeted:** v0.27.0 or later (needed for tag-UID linking). API facts below come from the v0.27.0 source (`spoolman/api/v1/{models,spool}.py`, `spoolman/tags.py`). Re-check if the server version differs.

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
GET /api/v1/spool?tag=<UID>
  200 + 1 spool  -> dashboard shows Spoolman spool: name, color, remaining_weight, location
  200 + []       -> "Not in inventory - Add?"  (flow 2)
  unreachable    -> show tag data only (today's behavior), status "Spoolman offline"
```

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
Blocked on the Milestone 2 investigation of how the K2 tracks remaining amount (`box/remain_material_data.json`?). Once that's known, push `remaining_weight` (or `use`) for the spool linked to the tag in each CFS slot.

## Device changes

- Config: `spoolman_url` (e.g. `http://homeassistant.local:7912`), set via the WiFi portal or Settings.
- `SpoolmanClient` (new): small blocking HTTP+JSON wrapper. Runs between RFID sessions, never inside one. Use ArduinoJson with the PSRAM allocator and a response filter.
- Replace `InventoryManager` usage in `ui_manager.cpp` (Scan Tag, Spool Detail, Custom Entry). Remove `InventoryManager` once the replacement works.
- Spool Detail "Write Tag" moves to the armed write flow at the same time.

## Phasing

1. Config + `SpoolmanClient` + lookup on auto-read (flow 1). Read-only, so it can't damage inventory data.
2. Add / link (flow 2) and post-write offer (flow 3).
3. Inventory screen (flow 4) and removal of `InventoryManager`.
4. Printer remaining-amount sync (flow 5), after the K2 investigation.

## Verification

Each phase is tested against the real Spoolman on the HA Pi, using a disposable test spool and tag. Confirm with Spoolman's web UI, not only the device's view of it.
