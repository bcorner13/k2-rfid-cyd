# Offline Inventory Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A SQLite copy of Spoolman on the SD card that fills the inventory screen, records every tag write as a pending change, and pushes those changes to Spoolman when it is reachable.

**Architecture:** Pure, host-tested headers decide everything: parsing Spoolman pages, matching filaments, the write prompts, and classifying HTTP results. `LocalStore` wraps the SQLite C API and is host-tested against macOS's system libsqlite3. On the board it runs on the SD card through `Sqlite3Esp32`, behind a FreeRTOS mutex. A new sync task owns all Spoolman traffic for the mirror (pull and push). The existing `SpoolmanClient` keeps doing live lookups by tag. The UI only takes the store lock briefly to read and record changes.

**Tech Stack:** ESP32-S3 Arduino (PlatformIO), LVGL 9.6, `siara-cc/Sqlite3Esp32@2.5` (SQLite C API), Arduino `SD` over SPI, ArduinoJson 7, Unity (`pio test -e native`), Docker `ghcr.io/donkie/spoolman:0.27.1` as a disposable test server.

**Spec:** `docs/superpowers/specs/2026-11-09-offline-inventory-design.md` (approved 2026-11-09). Read it before any task.

## Global Constraints

- Spoolman is the master. The device sends only its own pending changes. A full pull replaces the mirror.
- Tag read/write never depends on the SD card. With no card or a failed card, behaviour is as today, plus the visible note "No SD card - offline mode unavailable".
- Neither the UI loop nor `rfid_task` ever waits on the network. They wait on the SD card only for one short locked call (lock wait ≤ 50 ms; if busy, show "Updating..." and retry on the next tick).
- A tag write is never lost. The pending row is committed (`synchronous=FULL`) before the UI reports success.
- Every UID is normalized with `spoolman::normalizeUid` (uppercase hex, no separators) before storage or comparison.
- The CFS id lives in Spoolman filament extra field **`cfs_material_id`** (text). The device's create marker lives in spool extra field **`k2rfid_change`** (text). Extra values are JSON-encoded strings (`"\"08001\""`). (My call: the spec left it to the builder. `external_id` is left alone because SpoolmanDB uses it.)
- The device never writes `cfs_material_id` into a filament it didn't create.
- Pull pages are `limit=50&offset=N`, looping until a page returns fewer than 50 items. Don't rely on the `x-total-count` header.
- Sync timing (my call): every 5 min while online, immediately after a local change. Offline back-off 30 s → 60 s → 120 s → 300 s cap. Reset on success.
- Timestamps: NTP via `configTime` with TZ `EST5EDT,M3.2.0,M11.1.0` (my call). With no valid clock (year < 2026), show "synced N min ago" from `millis()`, or "never synced".
- Flashing: firmware only, `pio run -t upload --upload-port <port>`. **Never `uploadfs`** (it wipes `config.json`). **Tell Bradley before every flash or reset**, as its own message.
- Commit only when Bradley says so. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Branch `feat/spoolman-lookup`.
- Live Spoolman tests use a disposable spool and tag, and are confirmed in Spoolman's web UI (integration doc, "Testing"). No writes to the real Pi's Spoolman until Task 8's checkpoint.

## Review Focus

1. **A pull while a local change is still pending**: Spoolman's copy doesn't have the change yet, so a naive replace would make the spool's tag link vanish from the inventory until the push. The expected behaviour is that pending changes are reapplied on top after every pull. Test: `test_replace_mirror_reapplies_pending_link` (Task 3).
2. **Power cut after Spoolman accepted the create but before the row was marked done**: the next sync must find the spool by its `k2rfid_change` key and not create a second one. Tests: `test_resend_requires_check_when_sent` (Task 7) and `test_mark_sent_survives_reopen` (Task 3).
3. **A different tag placed after the confirm dialog** (the overwrite flow asks you to lift and place again): the decision was made for the first UID, so it must be made again. Test: `test_decision_rerun_on_uid_change` (Task 6).
4. **Spoolman (or a captive portal) returns HTML, truncated JSON, or a 500 halfway through the pages**: the pull is aborted and the mirror is left unchanged, never half-replaced. Tests: `test_parse_spool_page_malformed` (Task 2) and `test_failed_pull_leaves_mirror` (Task 3).
5. **The SD card fails or is pulled mid-session**: the tag write still succeeds, the note appears, and nothing hangs. Manual check in Task 6, step "SD pulled".

---

### Task 0: Preconditions (Bradley's calls, before any code)

- [ ] **Step 1: Ask Bradley to commit the pre-existing Spoolman lookup work** (`spoolman_client.*`, `spoolman_parse.h`, `test_spoolman_parse`, the config/network URL field, the `labelSpoolman` line). This plan builds on it, and the earlier split kept it uncommitted. Don't commit it without his yes.
- [ ] **Step 2: Start a disposable Spoolman on the Mac**

Run: `docker run -d --name spoolman-test -p 7912:8000 -v "$PWD/.spoolman-test:/home/app/.local/share/spoolman" ghcr.io/donkie/spoolman:0.27.1`, then `curl -s localhost:7912/api/v1/info`.
Expected: JSON with `"version": "0.27.1"`. Add `.spoolman-test/` to `.gitignore`. Point the device at `http://<mac-lan-ip>:7912` through the WiFiManager "Spoolman URL" field. Note the real URL first so it can be restored.

### Task 1: SD + SQLite on the real board (go/no-go)

**Files:**
- Create: `test/sd_sqlite/test_sd_sqlite.cpp` (a standalone sketch, like `test/i2c_scan`)
- Modify: `platformio.ini` (add `[env:sdtest]`; add `siara-cc/Sqlite3Esp32 @ 2.5.0` to `env:matouch_s3_43` `lib_deps`)

**Interfaces:** Produces only measurements, recorded in the spec's Constraints section.

- [ ] **Step 1: Add `[env:sdtest]`**: `extends = env:matouch_s3_43`, `build_src_filter = +<../test/sd_sqlite/test_sd_sqlite.cpp> -<*>`.
- [ ] **Step 2: Write the sketch.** `SPIClass sdSpi(FSPI); sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS); SD.begin(PIN_SD_CS, sdSpi, 20000000, "/sd")`. Open `/sd/k2rfid_test.db`. `PRAGMA journal_mode=TRUNCATE; PRAGMA synchronous=FULL;` (WAL needs shared memory, which this VFS doesn't have). Create table `t(seq INTEGER PRIMARY KEY, v TEXT)`. Then:
  (a) log `PRAGMA integrity_check` and `SELECT count(*), max(seq)`;
  (b) time one committed insert, ×20 (latency of a single pending row);
  (c) time 500 inserts in one transaction (size of a pull);
  (d) log internal heap and PSRAM before open, after open, and after (c);
  (e) loop forever inserting one committed row per 200 ms and printing the seq.
- [ ] **Step 3: Tell Bradley you're about to flash and reset, then flash**: `pio run -e sdtest -t upload --upload-port <port>`. Capture serial.
Expected: SD mounts, integrity `ok`, numbers printed.
- [ ] **Step 4: Power-cut test.** Ask Bradley to pull USB power during (e), three times, and to tell you before plugging back in. After each boot, (a) must print `ok`, and `max(seq)` must be ≥ the last seq printed before the cut minus 1.
- [ ] **Step 5: Checkpoint with Bradley.** Report the numbers. Pass = integrity ok on all three cuts, single insert < 100 ms, internal heap cost < 40 kB. On failure, **stop**: the JSON-lines fallback is Bradley's call. Record the results in the spec's Constraints, and the working SD/SQLite setup in `CLAUDE.md` under Hardware Notes.
- [ ] **Step 6: Tell Bradley before reflashing the main firmware** (`pio run -t upload ...`) so the board is back to normal.

### Task 2: Parse Spoolman pages into mirror rows (pure)

**Files:**
- Create: `include/spoolman_mirror_parse.h`
- Create: `test/test_spoolman_mirror/test_main.cpp`
- Modify: `platformio.ini` `[env:native] test_filter` (add `test_spoolman_mirror`)

**Interfaces:**
- Produces (namespace `spoolman`):
```cpp
struct MirrorFilament { int id; int vendorId; std::string vendor, name, material;
  std::string colorHex;              // "RRGGBB" uppercase, "" if none
  std::vector<std::string> multiColors;  // each "RRGGBB"; empty unless 2+
  std::string direction;             // "coaxial" | "longitudinal" | ""
  std::string cfsId;                 // decoded extra.cfs_material_id, "" if absent
  float weightG; int nozzleC; int bedC; };
struct MirrorSpool { int id; int filamentId; float initialG; bool hasRemaining; float remainingG;
  std::string location; bool archived; std::string changeKey;  // decoded extra.k2rfid_change
  std::vector<std::string> uids; };   // normalized
enum class PageResult : uint8_t { Ok, Malformed };
PageResult parseFilamentPage(const char* json, size_t len, std::vector<MirrorFilament>& out, size_t& count);
PageResult parseSpoolPage(const char* json, size_t len, std::vector<MirrorSpool>& out, size_t& count);
std::string normalizeHex(const char* in);   // strips '#', uppercases, keeps first 6; "" if invalid
std::string decodeExtraString(const char* raw);  // "\"08001\"" -> "08001"; non-string JSON -> ""
```
`count` = items on this page (the caller stops when count < 50). Append to `out`. Never clear it.

- [ ] **Step 1: Write failing tests** with v0.27.1-shaped fixtures (null fields omitted, as in `test_spoolman_parse`):
  - `test_parse_filament_single_color`: `color_hex:"e53935"` → `colorHex=="E53935"`, `multiColors` empty.
  - `test_parse_filament_multi_coaxial`: `multi_color_hexes:"C62828,C9A227"`, `multi_color_direction:"coaxial"` → 2 colours, `direction=="coaxial"`.
  - `test_parse_filament_cfs_extra`: `extra:{"cfs_material_id":"\"08001\""}` → `cfsId=="08001"`. `extra:{}` → `""`.
  - `test_parse_spool_tags_and_archived`: spool with `tags:[{"uid":"04:a2:b3:c4"}]`, `archived:true` → `uids[0]=="04A2B3C4"`, `archived`.
  - `test_parse_spool_no_remaining`: `remaining_weight` omitted → `hasRemaining==false`.
  - `test_parse_spool_page_malformed`: `"<html>"` and a truncated array → `Malformed`, `out` unchanged.
  - `test_parse_page_count`: a 3-item array → `count==3`.
- [ ] **Step 2: Run** `pio test -e native -f test_spoolman_mirror`. Expected: FAIL (header missing).
- [ ] **Step 3: Implement the header.** Use ArduinoJson filters as `parseLookup` does, so a 50-spool page stays small. The spool's `filament` object is read only for `id`. Nested vendor/filament data comes from the filament pull.
- [ ] **Step 4: Run** `pio test -e native`. Expected: all suites PASS.
- [ ] **Step 5: Commit** (when Bradley asks): `feat(sync): parse Spoolman filament/spool pages for the local mirror`.

### Task 3: LocalStore (SQLite schema, mirror, pending queue)

**Files:**
- Create: `include/local_store.h`, `src/local_store.cpp`, `test/test_local_store/test_main.cpp`
- Modify: `platformio.ini` `[env:native]`: `build_src_filter = +<local_store.cpp>`, `test_build_src = yes`, `build_flags` add `-lsqlite3`, `test_filter` add `test_local_store`

**Interfaces:**
- Consumes: `spoolman::MirrorFilament`, `spoolman::MirrorSpool` (Task 2).
- Produces:
```cpp
struct SpoolRow { int id;            // < 0 = created on this device, not yet in Spoolman
  int filamentId; std::string vendor, name, material, colorHex, direction, cfsId, location, uid;
  std::vector<std::string> multiColors; float initialG; bool hasRemaining; float remainingG;
  bool archived; bool pending; };   // pending = an unsynced change touches this spool
struct NewSpool { std::string cfsId, vendor, name, material, colorHex; float initialG; int nozzleC, bedC; };
enum class ChangeKind : uint8_t { CreateSpool, LinkTag };
enum class ChangeStatus : uint8_t { Pending, Sent, Done, NeedsAttention };
struct PendingChange { int64_t seq; ChangeKind kind; std::string uid, changeKey, lastError;
  int spoolId;            // LinkTag target (may be < 0); CreateSpool: its local id
  int replacesSpoolId;    // CreateSpool via "Replace spool #N", else 0
  int remoteSpoolId;      // set once Spoolman returned an id
  bool moveTag;           // LinkTag: Bradley chose "Move tag", so the push may take the UID off its holder
  NewSpool spool; ChangeStatus status; int attempts; };
class LocalStore {
public:
  bool open(const char* path);   // creates schema v1, sets journal_mode=TRUNCATE, synchronous=FULL
  void close(); bool isOpen() const;
  bool replaceMirror(const std::vector<spoolman::MirrorFilament>&, const std::vector<spoolman::MirrorSpool>&,
                     int64_t pulledAtEpoch);   // one transaction, then reapplies non-Done changes
  bool listSpools(std::vector<SpoolRow>& out, bool includeArchived);   // ordered vendor, name, id
  bool getSpool(int id, SpoolRow& out);
  bool findSpoolByUid(const char* uid, SpoolRow& out);
  bool listFilaments(std::vector<spoolman::MirrorFilament>& out);
  int  queueCreateSpool(const NewSpool&, const char* uid, int replacesSpoolId);  // returns local id (<0), 0 on error
  bool queueLinkTag(int spoolId, const char* uid, bool moveTag);
  bool pendingChanges(std::vector<PendingChange>& out);   // non-Done, seq order
  bool markSent(int64_t seq); bool setRemoteSpoolId(int64_t seq, int id);
  bool markDone(int64_t seq); bool markAttention(int64_t seq, const char* why); bool discard(int64_t seq);
  int  pendingCount(); int attentionCount();
  int64_t lastPullEpoch();   // 0 = never
};
```
Schema v1 (my call): `meta(key PK, value)` holding `schema_version`, `db_id` (random 32-bit hex, created once), `last_pull`; `filament(...)` matching `MirrorFilament`, with `multi_colors` stored comma-joined; `spool(...)` holding `SpoolRow`'s display columns denormalized from its filament; `tag(uid PK, spool_id)`; `pending(seq INTEGER PRIMARY KEY AUTOINCREMENT, kind, uid, spool_id, replaces_spool_id, remote_spool_id, payload_json, change_key, status, attempts, last_error)`. `change_key = "<db_id>-<seq>"`, so a fresh card never reuses a key. The local id of a CreateSpool is `-seq`.

Local apply, used both when queueing and when reapplying after a pull:
- **CreateSpool:** insert a spool row with id `-seq`, `pending=1`, then move the tag to it.
- **Moving a tag:** delete any `tag` row for the UID, then insert `(uid, spoolId)`.
- **LinkTag:** move the tag to `spoolId` and set `pending=1`.

- [ ] **Step 1: Write failing tests** (each opens `":memory:"`, or a temp file for reopen tests):
  - `test_open_creates_schema_and_db_id`: reopening a file keeps the same `db_id`.
  - `test_replace_mirror_lists_spools`: 2 filaments + 3 spools (1 archived) → `listSpools(false)` gives 2 rows with vendor/name/colour filled from the filament.
  - `test_find_by_uid`: after the pull, `findSpoolByUid("04A2B3C4")` finds the right id.
  - `test_queue_create_visible_immediately`: `queueCreateSpool` returns -1. `listSpools` has id -1 with `pending`. `findSpoolByUid` gives -1.
  - `test_queue_link_moves_tag`: UID on spool 5, `queueLinkTag(7, uid, false)` → `findSpoolByUid` gives 7.
  - `test_replace_mirror_reapplies_pending_link`: queue a link 7←UID, then a pull where Spoolman has no tag on 7 → still finds 7, `pending`.
  - `test_replace_mirror_keeps_local_spool`: queue a create, pull → spool -1 still listed.
  - `test_done_change_not_reapplied`: `markDone`, pull without the link → the link is gone (Spoolman wins).
  - `test_failed_pull_leaves_mirror`: no call to `replaceMirror` → `listSpools` is unchanged. Also `replaceMirror` with a filament id the spool points to but missing → returns true, row kept with empty name (no partial state).
  - `test_mark_sent_survives_reopen`: `markSent`, close, reopen → status `Sent`, attempts 1.
  - `test_attention_and_discard`: `markAttention` → `attentionCount()==1`, the change is still in `pendingChanges`. `discard` → gone, and its local apply disappears on the next pull.
- [ ] **Step 2: Run** `pio test -e native -f test_local_store`. Expected: FAIL to compile.
- [ ] **Step 3: Implement `src/local_store.cpp`** with the SQLite C API only (`sqlite3_prepare_v2`/`bind`/`step`). No Arduino headers, so the same file builds on the Mac and the board. `payload_json` holds `NewSpool` through ArduinoJson.
- [ ] **Step 4: Run** `pio test -e native`. Expected: all PASS.
- [ ] **Step 5: Run** `pio run`. Expected: the device build links against Sqlite3Esp32.
- [ ] **Step 6: Commit** (when asked): `feat(store): SQLite local copy of Spoolman with pending changes`.

### Task 4: Mount, lock, and pull on the board

**Files:**
- Create: `include/sd_storage.h`, `src/sd_storage.cpp`, `include/sync_state.h`, `include/spoolman_sync.h`, `src/spoolman_sync.cpp`
- Modify: `src/main.cpp` (setup: after `config.init()`, `sdStorage.begin()`, then `spoolmanSync.init()`), `src/network_manager.cpp` (call `configTime` once on Wi-Fi connect)

**Interfaces:**
- Consumes: `LocalStore` (Task 3), `parseFilamentPage`/`parseSpoolPage` (Task 2), `config.data.spoolman_url`.
- Produces:
```cpp
class SdStorage { public: bool begin();   // SPI pins as Task 1; opens /sd/k2rfid.db
  bool available() const;                 // false = no card / open failed / an I/O error since
  void markFailed(const char* why);       // any store call that returns false on the board calls this
  const char* failure() const;
  LocalStore* lock(uint32_t waitMs);  void unlock(); };   // FreeRTOS mutex; nullptr if busy or unavailable
extern SdStorage sdStorage;
struct StoreGuard { explicit StoreGuard(uint32_t waitMs = 50); ~StoreGuard(); LocalStore* operator->(); explicit operator bool() const; };
// SyncState lives in include/sync_state.h (pure, no Arduino), so Task 5's host tests can use it:
// enum class SyncState : uint8_t { NoStore, NeverSynced, Online, Offline, Syncing };
class SpoolmanSync { public: void init();   // task "spoolsync", 12 KB internal stack, core 0, prio 1
  void kick();                              // a local change happened: sync now
  SyncState state() const; int64_t lastPullEpoch() const; uint32_t lastPullMillis() const; };
extern SpoolmanSync spoolmanSync;
```
Pull (in the task): GET `/api/v1/filament?limit=50&offset=N`, then `/api/v1/spool?allow_archived=true&limit=50&offset=N`, into PSRAM-backed vectors. Use the same timeouts as `SpoolmanClient` (2 s connect, 3 s read). Max 64 KB per page. Any non-200 code or `Malformed` result → abort, mirror untouched, state `Offline` (code < 0) or the error logged (otherwise). On success, take the lock with `portMAX_DELAY` and call `replaceMirror`.

- [ ] **Step 1: Implement `SdStorage` and `StoreGuard`.** A missing card logs `SD: no card - offline mode unavailable` and leaves `available()==false`. Boot continues either way.
- [ ] **Step 2: Implement `SpoolmanSync`** (pull only for now; push comes in Task 8). Wait on `ulTaskNotifyTake` with the back-off or 5-min interval from Global Constraints. Log each pull: `Sync: pulled %u filaments, %u spools in %lu ms`.
- [ ] **Step 3: Build** with `pio run`. Expected: SUCCESS.
- [ ] **Step 4: Tell Bradley you're about to flash, then flash.** Against the Docker Spoolman, with 2 filaments and 3 spools made in its web UI, the log shows a pull with matching counts.
- [ ] **Step 5: Stop the container** (`docker stop spoolman-test`). The log shows the state go `Offline` with back-off. The UI stays responsive (tap between screens during the timeouts). Restart it and confirm the pull comes back.
- [ ] **Step 6: Commit** (when asked): `feat(sync): mount SD and pull Spoolman into the local copy`.

### Task 5: Inventory screen and spool detail from the local copy

**Files:**
- Modify: `include/ui/screens/screen_inventory.h`, `src/ui/screens/screen_inventory.cpp`, `include/ui/screens/screen_spool_detail.h`, `src/ui/screens/screen_spool_detail.cpp`, `src/ui/ui_manager.cpp` (`showInventoryScreen`, row tap, detail buttons), `include/ui/ui_manager.h`
- Create: `include/sync_status_text.h`, `test/test_sync_status_text/test_main.cpp` (+ `test_filter`)

**Interfaces:**
- Consumes: `StoreGuard`, `SpoolRow`, `spoolmanSync.state()/lastPullEpoch()/lastPullMillis()`.
- Produces:
  - `void formatSyncStatus(SyncState s, int64_t pullEpoch, uint32_t pullMillis, uint32_t nowMillis, int64_t nowEpoch, char* out, size_t n)` (pure; includes `sync_state.h`).
  - `ScreenInventory::populate(const std::vector<SpoolRow>&, const char* statusText)`.
  - `ScreenSpoolDetail::loadSpool(const SpoolRow&)`, `int currentSpoolId`.
- [ ] **Step 1: Failing tests for `formatSyncStatus`**:
  - `Online`, epoch known → `"Live · synced 14:05"`;
  - `Offline`, epoch known → `"Offline · synced Nov 9 14:05"`;
  - `Offline`, no epoch, pulled 12 min ago → `"Offline · synced 12 min ago"`;
  - `NeverSynced` → `"Never synced"`;
  - `NoStore` → `"No SD card - offline mode unavailable"`.
- [ ] **Step 2: Run** `pio test -e native -f test_sync_status_text`, expect FAIL. Implement it, then run again and expect PASS.
- [ ] **Step 3: Rewrite the inventory screen.**
  - Remove the Scan Tag and Add Custom buttons. Put the status line in the action bar.
  - Each row shows: a colour swatch (a 2- or 3-stripe swatch when `multiColors` is set), `vendor name`, `material`, `remaining g` (or "?"), location, a `●` pending marker in amber `0xB26A00` when `pending`, and `#id`, or `new` when id < 0.
  - The count label reads "N spools".
  - Store each row's spool id in its `user_data`, not its index.
  - `showInventoryScreen` reads `listSpools(false)` under a `StoreGuard`. If the lock is busy, it shows "Updating..." and retries on `spoolmanTick`.
- [ ] **Step 4: Rewrite the spool detail screen.**
  - It is read-only: swatch, name, vendor, material, initial/remaining g, location, tag UID ("no tag"), and the pending marker.
  - Delete the Update Weight, Unlink, Archive and Delete buttons and their weight modal (non-goal: no editing on the device). Keep Back and **"Write tag for this spool"**. Leave the write button as a stub until Task 6.
- [ ] **Step 5: Build, tell Bradley, flash.** Against Docker with 3 spools, the inventory lists 2 (one archived). The detail screen opens on tap. Stop Docker, reboot (tell Bradley first): the list still shows, with "Offline · synced ...".
- [ ] **Step 6: Ask Bradley to check the screen** on the device (layout is his eye, not the log's).
- [ ] **Step 7: Commit** (when asked): `feat(ui): inventory screen lists spools from the local copy`.

### Task 6: Write intents and prompts; recording changes

**Files:**
- Create: `include/write_intent.h`, `test/test_write_intent/test_main.cpp` (+ `test_filter`)
- Modify: `src/ui/ui_manager.cpp` (`armWrite`, `onTagForWrite`, the dialogs, detail "Write tag"), `include/ui/ui_manager.h`

**Interfaces:**
- Consumes: `StoreGuard`, `LocalStore::findSpoolByUid/queueLinkTag/queueCreateSpool`, `filamentDB`, `cfs::snapWeight`, `spoolmanSync.kick()`.
- Produces:
```cpp
enum class IntentKind : uint8_t { NewSpool, LinkExisting };
struct WriteIntent { IntentKind kind; int spoolId; };     // spoolId for LinkExisting
enum class PreWrite : uint8_t {
  Write,                 // no prompt; record per intent after a successful write
  WriteNoRecord,         // tag already linked to this spool: rewrite only
  AskReplaceOrRewrite,   // NewSpool on a tag linked to spool #linked
  AskMoveTag,            // LinkExisting(N) on a tag linked to spool #linked != N
};
PreWrite decidePreWrite(const WriteIntent&, bool storeAvailable, int linkedSpoolId /*0 = none*/);
bool needsAddToInventoryPrompt(const WriteIntent&, PreWrite, bool storeAvailable);  // NewSpool+Write only
struct DecisionMemo { char uid[spoolman::UID_BUF]; PreWrite decision; int choice; };
bool memoAppliesTo(const DecisionMemo&, const char* uid);   // false -> decide again
```
- [ ] **Step 1: Failing tests**:
  - `test_new_on_unlinked_writes_and_prompts`: `NewSpool`, linked 0 → `Write`, and `needsAddToInventoryPrompt` true.
  - `test_new_on_linked_asks_replace`: linked 12 → `AskReplaceOrRewrite`.
  - `test_link_same_spool_rewrite_only`: `LinkExisting(12)`, linked 12 → `WriteNoRecord`.
  - `test_link_other_spool_asks_move`: `LinkExisting(12)`, linked 5 → `AskMoveTag`.
  - `test_no_store_plain_write`: `storeAvailable=false` → `Write`, and no prompt (as today).
  - `test_decision_rerun_on_uid_change`: a memo for `04A2B3C4` doesn't apply to `11223344`.
- [ ] **Step 2: Run, expect FAIL. Implement. Run, expect PASS.**
- [ ] **Step 3: Wire up `onTagForWrite`.**
  - **Before writing:** `findSpoolByUid` → `decidePreWrite`. Skip it if a memo applies to this UID.
  - **Ask dialogs** (msgboxes; set `writeConfirmPending` while open). Each choice stores a memo and calls `armWrite(true)`. Copy is exact:
    - Replace or rewrite: title "Tag is on spool #N"; buttons "Replace with a new spool" / "Just rewrite the tag" / "Cancel".
    - Move tag: title "Tag is on spool #M"; text "Move it to spool #N?"; buttons "Move tag" / "Cancel".
  - **After a successful write:**
    - New spool, plain write: show "Add to inventory?" with "Add" (the default, first button) / "No". Add → `queueCreateSpool(NewSpool from currentSpool + its FilamentProfile, uid, 0)`.
    - Replace → `queueCreateSpool(..., uid, linked)`.
    - Link existing → `queueLinkTag(N, uid, false)`; after a "Move tag" choice → `queueLinkTag(N, uid, true)`.
    - Then `spoolmanSync.kick()`. Status: "Written - added to inventory (syncs when Spoolman is up)".
  - **If the store call fails:** keep the written tag, call `sdStorage.markFailed`, and show "Tag written - NOT recorded (SD error)" in red.
- [ ] **Step 4: Wire up the detail screen's "Write tag".**
  - Build `SpoolData` from the `SpoolRow`: material id = `cfsId`, or the `filamentDB` brand+name match; colour = `colorHex`, or the first of `multiColors`; weight = `cfs::snapWeight(initialG)`.
  - Set the intent to `LinkExisting(id)` and `armWrite(false)`.
  - With no material id, switch to the main screen pre-filled with colour and weight. Keep the intent `LinkExisting(id)` and show "Pick the product for spool #N". The Write button keeps that intent until the write completes, a cancel or timeout happens, or the screen changes to Inventory.
- [ ] **Step 5: Build, tell Bradley, flash. Hardware checks** (one tag in the field):
  - (a) Blank tag from the main screen → Add → a `new` row with ● in the inventory.
  - (b) The same tag again → Replace / Rewrite dialog.
  - (c) Inventory spool → write → that row shows the UID with ●.
  - (d) The tag from (c), written for a different inventory spool → Move dialog.
  - (e) "No" on Add → nothing recorded.
  - (f) **SD pulled** (ask Bradley to remove the card at runtime): a write still succeeds, the red "NOT recorded" message shows, nothing hangs, and later screens say "No SD card".
  - Reboot with the card back in (tell Bradley first): the changes from (a)–(d) are still there.
- [ ] **Step 6: Commit** (when asked): `feat(write): record tag writes as pending inventory changes`.

### Task 7: Sync plan (pure): filament matching, request bodies, result classes

**Files:**
- Create: `include/spoolman_sync_plan.h`, `test/test_spoolman_sync_plan/test_main.cpp` (+ `test_filter`)

**Interfaces:**
- Consumes: `MirrorFilament`, `NewSpool`, `PendingChange`.
- Produces (namespace `spoolman`):
```cpp
int matchFilament(const std::vector<MirrorFilament>&, const NewSpool&);  // 0 = none
//   1) cfsId equal AND colour set equal; 2) vendor+name (case-insensitive) AND colour set equal.
//   Colour set equal = single colorHex equal, or the same ordered multiColors and the same direction.
//   A multi-colour filament never matches a single-colour NewSpool.
int matchVendor(const std::vector<MirrorFilament>&, const char* vendorName);  // vendorId, 0 = none
std::string vendorBody(const char* name);
std::string filamentBody(const NewSpool&, int vendorId);  // name, material, color_hex, density 1.24, diameter 1.75,
                                                          // settings_extruder_temp, settings_bed_temp,
                                                          // extra.cfs_material_id (JSON-encoded string)
std::string spoolBody(int filamentId, float initialG, const char* changeKey);  // extra.k2rfid_change
enum class Outcome : uint8_t { Done, Offline, RetryLater, NeedsAttention };
Outcome classify(int httpCode);                  // <0 Offline; 2xx Done; 409/400-499 NeedsAttention; 5xx RetryLater
std::string attentionText(int httpCode, const char* body);  // 409 -> "Tag is on another spool in Spoolman"
bool mustCheckBeforeResend(const PendingChange&);   // status Sent && remoteSpoolId == 0
int findByChangeKey(const std::vector<MirrorSpool>&, const char* key);   // spool id, 0 = none
```
- [ ] **Step 1: Failing tests**:
  - `test_match_by_cfs_and_color`: same cfsId, other colour → 0; same colour → id.
  - `test_match_vendor_name_fallback`: no cfsId on the filament, "creality"/"ender-pla" vs "Creality"/"Ender-PLA" with the same colour → id.
  - `test_multi_never_matches_single`.
  - `test_multi_direction_matters`: the same colours, coaxial vs longitudinal → 0.
  - `test_filament_body_encodes_extra`: the body contains `"cfs_material_id":"\"08001\""`.
  - `test_spool_body_has_change_key`.
  - `test_classify_codes`: -1, 200, 201, 409, 422, 500.
  - `test_resend_requires_check_when_sent`.
  - `test_find_by_change_key`.
- [ ] **Step 2: Run, expect FAIL. Implement. Run, expect PASS.**
- [ ] **Step 3: Commit** (when asked): `feat(sync): pure push planning - matching, bodies, outcomes`.

### Task 8: Push to Spoolman

**Files:**
- Modify: `src/spoolman_sync.cpp`, `docs/architecture/spoolman-integration.md` (the endpoints as confirmed)

**Interfaces:**
- Consumes: everything in Task 7, `LocalStore::pendingChanges/markSent/setRemoteSpoolId/markDone/markAttention`.
- Produces: nothing new; `SpoolmanSync` now pushes before each pull.

- [ ] **Step 1: Confirm the endpoints in the v0.27.1 source** (`github.com/Donkie/Spoolman`, tag `v0.27.1`, `spoolman/api/v1/`). Check and record in the integration doc, with file and line:
  - the tag-link route and its body;
  - how a tag is removed from a spool (needed for "Replace" and "Move");
  - the extra-field registration route (`/field/{entity}/{key}`) and its body;
  - that `POST /spool` accepts `extra`;
  - the Origin/CORS check behind "Trusting writes ... own origin only".

  If something differs from this plan, the source wins: update this plan's Task 7 bodies in the same step.
- [ ] **Step 2: Origin check against Docker.** `curl -s -o /dev/null -w '%{http_code}' -X POST localhost:7912/api/v1/vendor -H 'Content-Type: application/json' -d '{"name":"origin-test"}'`, sent with no Origin header. If it's refused, retry with `-H 'Origin: http://<host>:7912'`. Record which works. If both are refused, **stop and ask Bradley** (spec: no Pi changes without asking). Delete the test vendor.
- [ ] **Step 3: Implement the push** in the sync task, before the pull, once per run:
  - Refresh filaments (GET pages, Task 2).
  - Register the extra fields `cfs_material_id` (filament) and `k2rfid_change` (spool) if they're missing. A "field exists" response counts as success.
  - Then for each `pendingChanges()` entry, in seq order:
    - **CreateSpool:**
      - If `mustCheckBeforeResend`, GET spool pages and run `findByChangeKey`; if found, take that id.
      - Otherwise `matchFilament`, or create the vendor (by `matchVendor`) and then the filament.
      - `markSent`, then `POST /spool`, then `setRemoteSpoolId`.
      - If `replacesSpoolId`, remove the UID from that spool.
      - Link the UID to the new id, then `markDone`.
    - **LinkTag** to a local id (< 0): resolve it through the CreateSpool change whose local id it is. If that one isn't done, skip with "waiting for new spool". Otherwise link: on a 409 the change needs attention, unless `moveTag` is set, in which case remove the UID from its current holder first.
  - **After each request:**
    - `Offline` or `RetryLater` → end the run (back-off).
    - `NeedsAttention` → `markAttention(seq, attentionText(...))`, then go on to the next change.
  - Needs-attention changes are attempted again on every run (spec, my call).
  - Send the Origin header only if Step 2 showed it's needed.
- [ ] **Step 4: Build, tell Bradley, flash. Live checks against Docker** (confirm each in its web UI):
  - (a) Offline: changes from Task 6, then start Docker → spools exist, the filament has `cfs_material_id`, tags are linked, ● markers clear after the pull.
  - (b) Duplicate check: ask Bradley to pull power right after the "POST /spool" log line, then reboot (tell him first) → exactly one spool with that `k2rfid_change`.
  - (c) 409: link a tag to a spool in the web UI, then write that tag for a different spool from the inventory screen on the device with the store offline → after sync, needs attention with "Tag is on another spool in Spoolman", and the changes behind it still synced.
  - (d) Red/gold coaxial filament in the web UI, written from the inventory screen → the tag gets the first colour (check by reading it back), and no new filament is created.
- [ ] **Step 5: Checkpoint with Bradley** before pointing the device at the real Pi's Spoolman: show what will be pushed (`pendingChanges` from the log), restore his URL, and run one sync once he says yes.
- [ ] **Step 6: Commit** (when asked): `feat(sync): push pending tag writes to Spoolman`.

### Task 9: Sync status in Settings; main-screen offline fallback

**Files:**
- Modify: `include/ui/screens/screen_settings.h`, `src/ui/screens/screen_settings.cpp`, `src/ui/ui_manager.cpp` (`spoolmanTick`, settings handlers)

**Interfaces:**
- Consumes: `formatSyncStatus`, `LocalStore::pendingCount/attentionCount/pendingChanges/discard/findSpoolByUid`, `spoolman::formatSummary`.
- Produces: `ScreenSettings::showSync(const char* status, int pending, const std::vector<PendingChange>& attention)`.

- [ ] **Step 1: Settings, "Inventory sync" section:**
  - the status line (`formatSyncStatus`);
  - "N changes waiting";
  - a "Sync now" button (`spoolmanSync.kick()`);
  - one row per needs-attention change: "Spool #id / UID: reason", each with a **Discard** button;
  - "No SD card - offline mode unavailable" when there is no store.
- [ ] **Step 2: Main-screen Spoolman line.**
  - When the `SpoolmanClient` result is `Offline` (or `Error` from a timeout), look up `findSpoolByUid` in the local copy.
  - If found, show `formatSummary` plus a second-line suffix: " · offline, synced <time>" (and " · not synced" when `pending`). Use the grey colour, not green.
  - If not found, keep "Spoolman offline".
  - When `Found` live and the local row is `pending`, append " · not synced".
- [ ] **Step 3: Build, tell Bradley, flash.** Check against Docker, running and stopped: tag reads show the offline line. Settings shows the counts, and Discard removes an attention item.
- [ ] **Step 4: Commit** (when asked): `feat(ui): sync status in settings; offline Spoolman line from local copy`.

### Task 10: Retire the old inventory and update the docs

**Files:**
- Delete: `src/inventory_manager.cpp`, `include/inventory_manager.h`, and `screen_custom_entry.*` / `tag_data.*` if `grep` shows nothing else uses them after Task 5
- Modify: `src/main.cpp` (drop `inventory.init()`), `src/ui/ui_manager.cpp`, `docs/architecture/ADR-001-inventory-in-spoolman.md`, `docs/architecture/spoolman-integration.md`, `CLAUDE.md` (module map, data flow 5, SD in use), `TODO.md`

- [ ] **Step 1: `grep -rn "inventory\.\|SpoolRecord\|screenCustomEntry\|TagData" src include`**, then remove what only the old path used. `pio run` must succeed with no references left.
- [ ] **Step 2: ADR-001, open question 2:** add "Reversed 2026-11-09 by Bradley after a real outage: local SQLite copy on SD, Spoolman stays master. See `docs/superpowers/specs/2026-11-09-offline-inventory-design.md`."
- [ ] **Step 3: `CLAUDE.md`:**
  - add `LocalStore`, `SpoolmanSync` and `SdStorage` to the module map;
  - inventory data flow → the local copy;
  - SD card "used for `/sd/k2rfid.db`";
  - the Docker test server command under Environment / access.
- [ ] **Step 4: Run** `pio test -e native` and `pio run`. Expected: all PASS, build SUCCESS. Tell Bradley, then flash and do a final boot check (no `inventory.json` errors, inventory lists).
- [ ] **Step 5: Commit** (when asked): `refactor: retire inventory.json; record ADR-001 reversal`.
