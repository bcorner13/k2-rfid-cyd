# ADR-001: Inventory lives in Spoolman, not on the device

**Status:** Accepted. The decision dates from early/mid 2026 but was never recorded. It was re-confirmed and written down on 2026-10-29.
**Supersedes:** on-device inventory (`InventoryManager`, `/inventory.json`, FSD inventory sections, TODO Milestone 3 as written)

## Context

- The device (ESP32-S3, 800×480) is a poor home for a full inventory. Measured 2026-10-28/29: internal RAM ran down to ~2 KB until LVGL moved to PSRAM, the LittleFS partition is 3.4 MB, and the live printer material DB alone is 711 KB. The screen is cramped for editing lists.
- There will be more than one printer. A Prusa Core One L+ is on order. Its tags are OpenPrintTag, NFC-V, so a different reader would be needed.
- The on-device inventory never worked. As of 2026-10-29, Inventory → Scan Tag calls `readTag()`, which expects a "K2PF" layout that real tags don't use, and it never creates a record for a new tag. The inventory list is always empty.
- **Spoolman** (github.com/Donkie/Spoolman) is an open-source filament inventory with SQLite by default, a web UI and a REST API. Since **v0.27.0 (2026-09-25)** it links physical NFC/RFID tags to spools by **hardware UID**:
  - `POST /api/v1/spool/{id}/tag` with `{uid, format}`. Returns 409 and the current holder if the UID is already linked.
  - `GET /api/v1/spool?tag=<uid>` finds the spool for a UID. UIDs are normalized: uppercase hex, separators ignored.
  - `DELETE /api/v1/spool/{id}/tag/{uid}`
  - Known `format` names include `creality`, `openprinttag` and `prusa`. The field is informational only.
- Spoolman will run on the Home Assistant Raspberry Pi: always on, on the LAN.

## Decision

1. **Spoolman is the inventory system of record.** Spools, filaments, vendors, weights, history and editing all live in Spoolman.
2. **The device is the tag station and printer bridge.** It reads and writes CFS tags at the spool, looks spools up in Spoolman by tag UID, creates and links spools for new tags, and pushes printer-side information (e.g. remaining amount) to Spoolman.
3. **The tag's hardware UID is the link between a tag and a spool.** We do not encode a Spoolman ID on the tag. DnG K2-RFID puts the Spoolman spool ID in the 6-digit serial field, which predates Spoolman's native UID linking.
4. **No new work on the on-device inventory.** `InventoryManager` stays until it is replaced, then gets removed.

## Consequences

- The device needs the Spoolman base URL in config, plus HTTP+JSON client code. That is much simpler than SSH.
- Inventory features need Spoolman reachable. Behavior when it isn't reachable is open question 2.
- Multi-printer and multi-tag-format support (OpenPrintTag) comes from Spoolman for free. The device only has to report a UID and a format.

## Open questions

1. Where printer → Spoolman sync runs: on the device, or on the Pi/laptop (which can SSH to the printer directly, avoiding SSH on the ESP32).
2. Offline behavior: tag read/write must keep working without Spoolman. Should lookups and links be queued and replayed later?
3. Mapping CFS material id (`01001`, `P7003`, …) to a Spoolman filament. Candidates are `filament.external_id`, or an `extra` field such as `cfs_material_id`. Matching by vendor + name is the fallback.
4. Whether the K2's remaining-amount estimate is keyed to the tag serial or UID (TODO, Milestone 2). This affects whether the device should keep the serial stable on rewrites.
5. **Spoolman MCP server:** github.com/Disane87/spoolman-mcp (TypeScript, MIT; reviewed 2026-10-29 at the commit pushed 2026-08-15). It is a thin REST wrapper (axios) with 34 `spoolman_*` tools, configured by `SPOOLMAN_URL` and an optional `SPOOLMAN_API_KEY`. It has no shell or file access. It includes destructive `spoolman_delete_{spool,filament,vendor,field}`: gate those with `permissions.ask` after confirming the names on the loaded server. **It has no tag-UID tools** (it predates Spoolman v0.27), so tag linking still needs direct API calls. Useful for inspecting and curating inventory from Claude Code sessions.

Design details: `docs/architecture/spoolman-integration.md`.
