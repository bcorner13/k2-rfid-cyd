# Offline Inventory: Local Spoolman Mirror on SD — Design

**Date:** 2026-11-09
**Status:** Ready for Bradley's approval
**Amends:** [ADR-001](../../architecture/ADR-001-inventory-in-spoolman.md), open question 2 (decided 2026-11-05: "no local database"). Bradley reversed this on 2026-11-09 after a real outage.
**Builds on:** [spoolman-integration.md](../../architecture/spoolman-integration.md) flows 1–4

## Why

On 2026-11-09 Bradley wrote three CFS tags (Creality Ender-PLA red, Creality CR-Silk lavender,
Polymaker black). None of them reached Spoolman. Two reasons:

1. Spoolman runs on a Raspberry Pi 3 that is unreliable. That day it accepted TCP connections but
   answered no HTTP, and because the Pi is also the LAN's first DNS server, `spoolman.lan` stopped
   resolving.
2. The device has **no Spoolman write path at all**. Phase 1 is a read-only lookup. Even with
   Spoolman up, a write is never recorded.

The device needs to keep working when Spoolman is down and catch Spoolman up afterwards, without
Bradley re-entering anything.

## What it is

A **local copy of Spoolman's inventory**, held in SQLite on the board's SD card. **Spoolman is the
master.** The device:

- **Pulls** Spoolman's vendors, filaments, spools and tag links into the local copy whenever Spoolman
  is reachable.
- **Works from the local copy** when Spoolman is not reachable: tag lookups, the inventory screen,
  and picking a spool to write.
- **Records its own changes** (from tag writes) in a pending list in the same database, applies them
  to the local copy straight away, and **pushes only those** to Spoolman when it is reachable. After a
  push, the next pull overwrites the local copy, so Spoolman's version always wins in the end.

## The two write paths (Bradley's decision)

| Where the write starts | What it means | Pending change recorded |
|---|---|---|
| **Inventory screen**: pick a spool, "Write tag" | This physical tag belongs to **that existing spool** | Link tag UID → that spool |
| **Main screen**: Brand / Type / Product / colour / size, Write | A **new spool** of that filament | Create spool (filament, size), linked to the tag UID |

- From the inventory screen, the tag payload comes from the spool's filament (CFS material id,
  colour) and initial weight (snapped to a CFS size, `include/cfs_weight.h`). This is the armed-write
  flow already in the code, pre-filled.
- From the main screen, at sync time the device finds the matching Spoolman filament (by CFS material
  id, then vendor + name, see "Filament matching"). If none exists it creates one, plus its vendor if
  needed.
- Both happen whether Spoolman is up or down. Online, the push follows at once. Offline, it waits.

### Prompts before a main-screen write (Bradley, 2026-11-09: "rewrite / reuse prompts are fine")

- **Tag already linked** (the local copy has its UID on spool #N): the overwrite dialog offers
  **"Replace spool #N with a new spool"** or **"Just rewrite the tag (no inventory change)"**. The
  replace option queues create-spool plus moving the link from #N to the new spool. The old spool is
  not archived or deleted. That stays in Spoolman's UI (non-goal: no editing on the device).
- **Blank or unlinked tag:** ask **"Add to inventory?"**, defaulting to yes. "No" writes the tag and
  records nothing, which covers test and scratch writes.

### Inventory write when the spool's filament has no CFS material id (my call)

Match the filament to the device's material DB by vendor + name. If nothing matches, open the main
screen pre-filled with that spool's colour and size so Bradley picks the product, and record only a
link-tag change. The device never writes a CFS id into a Spoolman filament it didn't create.

## Goals

1. A tag write is **never lost**: its record survives Spoolman being down, Wi-Fi being down, and a
   power cut right after the write.
2. Tag reads, the inventory screen, and picking a spool all work with Spoolman unreachable, from the
   local copy.
3. When Spoolman comes back, pending changes reach it **without any action from Bradley**, in the
   order they happened.
4. The inventory screen (blank today: it reads the retired `inventory.json`, which nothing fills)
   lists spools from the local copy, and from there "Write tag for this spool" works.
5. It's always visible whether the device is showing live or cached data, and which spools have
   unsynced changes.

## Non-goals

- Editing spool fields on the device (weight, location, comments, archiving, deleting). Editing stays
  in Spoolman's web UI (unchanged from the integration doc).
- Two-way merging. The device never edits anything it didn't create or link itself.
- Printer → Spoolman remaining-amount sync (`remain_len` / measuring wheel, integration doc §5). This
  is a separate project. It will later reuse the pending list, so the schema should allow more change
  kinds.
- Several devices sharing one Spoolman. Only one writer device exists.
- Back-filling the three tags written on 2026-11-09. They predate this. Bradley links them by hand
  after the first sync, by picking each from the inventory screen and re-writing, or in Spoolman's UI.

## Anti-goals (would make it a failure even if it "works")

- **The device silently overwriting something Bradley changed in Spoolman.** The device only sends
  its own pending changes, and the pull always replaces the local copy.
- **Showing cached weights as if they were live.** Offline data is always labelled with when it was
  last synced. This was ADR-001's "stale weights shown as fact" concern.
- **The UI hanging on SD or network.** HTTP already runs on its own FreeRTOS task (`SpoolmanClient`).
  SD/SQLite work must not freeze the screen either.
- **Tag read/write depending on the SD card.** With no card, a broken card or a full card, tag
  read/write works exactly as today, with a visible "No SD card / offline mode unavailable" note.
- **A sync that loops on a bad item and blocks everything behind it.** See "Failures".

## How ADR-001's three objections are handled

| ADR-001 concern (2026-11-05) | This design |
|---|---|
| A second editable inventory | The local copy isn't editable. The only local changes are the device's own two kinds (create + link, link), queued as pending and replayed. |
| Conflict rules against Spoolman web edits | No merging. Spoolman is the master: push pending, then a full pull replaces the local copy. If a pending change is refused (e.g. 409, tag linked elsewhere), it's kept and shown, not forced. |
| Stale weights shown as fact | Every cached view carries "offline · synced <time>". Unsynced spools are marked. |
| "No SD or SQLite support today" | Still true. Step 1 of the build is a test on the real board. |

## Constraints (measured or from the repo)

- **Board:** MaTouch ESP32-S3 4.3" V3.1, 8 MB OPI PSRAM, 16 MB flash. Internal heap after boot is
  about 160–175 kB free (boot logs, 2026-11-09). Large buffers must go in PSRAM, as the JSON loader
  already does.
- **SD card:** SPI on MOSI 11 / SCK 12 / MISO 13 / CS 10 (`include/board_pins.h`). A 32 GB card ships
  with the board. **No firmware uses the SD card yet.** The display is RGB-parallel and the PN532 and
  touch are on I²C (17/18), so the SD's SPI bus is shared with nothing.
- **SQLite library:** `siara-cc/Sqlite3Esp32` 2.5 is the established ESP32 option in the PlatformIO
  registry. **It is untested on this board.** The first build step is a test that opens a database on
  this SD card, writes, survives a power cut mid-write, and reports RAM cost. If it fails, the
  fallback is an append-only JSON-lines pending log plus a JSON snapshot of the pull, still on SD.
  That's Bradley's call to revisit, not the builder's to switch to silently.
  **Measured 2026-11-09 (plan Task 1): pass.** 29.8 GB SDHC card. Single committed insert 12–17 ms
  average, worst 53 ms; 500 rows in one transaction 0.7–1.4 s; about 9 kB internal heap. Three USB
  power cuts during committed inserts: no rows lost and the full scan clean each time. Settings that
  matter: `PRAGMA page_size=4096` (new file only) and `cache_size=-256`, because the library's
  default is a 1 KB cache with 512 B pages (one 500-row transaction took 6.9 s), and 4 KB pages
  allocate from PSRAM; plus `journal_mode=TRUNCATE` and `synchronous=FULL`, since the library omits
  WAL. `PRAGMA integrity_check` is compiled out of `Sqlite3Esp32` (`SQLITE_OMIT_INTEGRITY_CHECK`).
  `SQLITE_NO_SYNC` in its config affects only SQLite's Unix/Windows file layers. Its ESP32 file layer
  still calls `fflush` and `fsync`, but it ignores fsync's return value.
- **Spoolman:** v0.27.1, no auth, plain HTTP on `:7912`. Endpoints already designed in the
  integration doc: `GET /spool?tag=<uid>&allow_archived=true`, `POST /spool`,
  `POST /spool/{id}/tag {uid, format:"creality"}` (409 if the UID is linked elsewhere), plus
  `GET /vendor`, `/filament`, `/spool` for the pull. The integration doc says to confirm that v0.27.1
  accepts a device `POST` with no `Origin` header ("trusting writes from own origin only" in its log)
  **before** building any push. This blocks the push half of the work.
- **DNS:** when the Pi is down, `spoolman.lan` doesn't resolve. "Offline" must cover DNS failure,
  connect failure, timeout and a server that accepts but never answers (all seen 2026-11-09).
- **Time:** the PCF8563 RTC has no driver yet. Offline changes are ordered by a local sequence number,
  not a clock. Timestamps are best effort (NTP when online).
- **Existing code to build on:** `SpoolmanClient` (own task, single-slot queues to the main loop,
  host-tested parsing in `spoolman_parse.h`), the armed-write flow and overwrite confirm in
  `ui_manager.cpp`, `FilamentDB` (material DB with CFS ids).

## The local database (shape, not final schema)

- **Mirror tables:** vendor, filament, spool, tag link. Copies of Spoolman's records with Spoolman's
  ids, plus the fields the screens need: name, vendor, material, colour (`color_hex` and
  `multi_color_hexes` / `multi_color_direction`, already in Spoolman, so the multi-colour work can use
  them later), initial and remaining weight, location, archived.
- **Locally created spools** exist in the mirror before Spoolman knows them. They need a local id that
  can't collide with Spoolman ids, and must be re-pointed to the real id once pushed.
- **Pending changes:** ordered by sequence. Kind (`create_spool`, `link_tag` now, more kinds later),
  what it needs to replay, status (pending / done / failed-needs-attention), attempts, last error.
- **Sync metadata:** last successful pull time, schema version.

## Sync behaviour

1. **Push before pull.** Replay pending changes oldest first. A change counts as done only when
   Spoolman has confirmed it.
2. **Then pull a full snapshot** and replace the mirror tables in one transaction, keeping locally
   created spools whose push hasn't been confirmed.
3. **When:** at boot, when Spoolman becomes reachable again, after each local change while online,
   and every few minutes while online (interval is the builder's call; Spoolman holds a home
   inventory of tens to a few hundred spools).
4. **Replay must be safe to repeat.** A power cut between "Spoolman accepted" and "marked done" must
   not create a duplicate spool on the next attempt. How (check before create, a marker in the
   spool's `extra`, …) is the builder's call. The requirement is no duplicates.

## Filament matching (create-spool push)

The order follows the integration doc, open question 3: the CFS material id stored on the Spoolman
filament (`external_id` or an `extra` field such as `cfs_material_id`; the builder picks one and
documents it), then vendor + name, then create the vendor and filament from the device's material DB
profile (name, material, colour, temps, CFS id). A filament the device creates carries the CFS id, so
the next match is exact.

**Colour is part of the match.** In Spoolman the colour lives on the filament, so a filament is one
colour variant of a product. A Spoolman filament has either one `color_hex`, or two or more
`multi_color_hexes` with a `multi_color_direction`:

- `coaxial`: the colours print at the same time (Bradley's red/gold, blue/green);
- `longitudinal`: the colour changes along the strand (rainbow / gradient).

So a match needs the CFS id (then vendor + name) **and** the same colour set and direction. With no
match, the device creates a new filament for that colour variant. The CFS tag only holds one
`RRGGBB`, so a multi-colour spool's tag carries its first colour. The full set lives in Spoolman.
Picking multi-colour on the device is a later project (colour picker parts 2–3). Until then, a
main-screen write is always single-colour, and inventory writes of multi-colour spools work through
the first-colour rule.

## Failures

- **Spoolman unreachable in any way:** stay offline, keep the pending list, retry with back-off. No
  error noise on screen beyond the offline marker.
- **Spoolman refuses one change** (409: tag already linked to another spool; 4xx validation): mark
  that change "needs attention", show it, and **carry on with the rest**. One bad item must not block
  the queue. Spoolman is the master, so the device never moves a tag off another spool by itself,
  except when Bradley chose "Replace spool #N" for that tag.
- **Resolving "needs attention" (my call):** retried automatically on every sync, in case the cause
  was fixed in Spoolman's web UI. Settings has a **Discard** button for each one.
- **Spoolman refuses device POSTs (Origin check, my call):** first send an `Origin` header matching
  the server, which is a device-only change. If that still fails, stop and ask Bradley before changing
  anything on the Pi.
- **SD missing, full or corrupt:** offline mode is off, a visible note appears, tag read/write works
  as today, and lookups go live-only (today's behaviour).

## Screens

- **Main screen, Spoolman line:** live result as today. When offline, the local-copy result with
  "offline · synced <time>". A pending marker if that spool has unsynced changes.
- **Inventory screen:** list from the local copy (name, colour swatch, remaining g, location), plus
  the same offline / synced-time marker and per-spool pending marker. Tap a spool, then "Write tag for
  this spool" runs the armed write, pre-filled. Replaces today's `inventory.json` source.
- **Somewhere visible** (Settings is fine): pending count, last sync time, and any "needs attention"
  items with their reason.

## Build order (my call)

The blank inventory screen is the problem Bradley feels most, so it comes first:

1. SQLite-on-SD test on the real board.
2. Pull into the local copy, and an **inventory screen that lists spools** from it. This is useful as
   soon as Spoolman answers once, and it keeps working after the Pi drops.
3. Inventory pick → write tag (the link-tag change).
4. Main-screen write → create spool, with the prompts above.
5. Push (gated on the Origin check), then the pending/needs-attention view in Settings.

## Left to the builder

The exact schema, thread/task ownership of the database (with the no-UI-freeze anti-goal), pull
paging, retry back-off and sync interval, how replays avoid duplicates, which Spoolman field holds the
CFS id, screen layout details, and host-testable splits (sync decisions belong in pure,
host-testable code like `spoolman_parse.h`).

## Also to do with this work

- Amend ADR-001 open question 2 to record the reversal and link this document.
- Retire the old `InventoryManager` / `inventory.json` path once the new inventory screen works
  (the integration doc already plans this).
