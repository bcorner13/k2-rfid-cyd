// Go/no-go test for offline inventory: SQLite on the MaTouch SD card.
// Plan: docs/superpowers/plans/2026-11-09-offline-inventory.md, Task 1.
// Build/flash: pio run -e sdtest -t upload --upload-port <port>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <sqlite3.h>
#include "board_pins.h"

static SPIClass sdSpi(FSPI);
static sqlite3* db = nullptr;

static void logMem(const char* tag) {
    Serial.printf("MEM[%s] internal=%u psram=%u minInternal=%u\n", tag,
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL), ESP.getFreePsram(),
                  heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}

static bool exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        Serial.printf("SQL ERROR %d on '%s': %s\n", rc, sql, err ? err : "?");
        sqlite3_free(err);
        return false;
    }
    return true;
}

static int printRow(void*, int n, char** vals, char** cols) {
    for (int i = 0; i < n; i++) Serial.printf("  %s=%s", cols[i], vals[i] ? vals[i] : "NULL");
    Serial.println();
    return 0;
}

static int64_t g_seq = 0;

static bool insertOne(const char* v) {
    sqlite3_stmt* st;
    if (sqlite3_prepare_v2(db, "INSERT INTO t(v) VALUES(?)", -1, &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, v, -1, SQLITE_STATIC);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { Serial.printf("insert rc=%d %s\n", rc, sqlite3_errmsg(db)); return false; }
    g_seq = sqlite3_last_insert_rowid(db);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(800);
    Serial.println("\n=== SD/SQLite test ===");
    logMem("boot");

    sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, sdSpi, 20000000, "/sd")) {
        Serial.println("RESULT: SD mount FAILED");
        return;
    }
    Serial.printf("SD: type=%d size=%llu MB used=%llu KB\n", SD.cardType(),
                  SD.cardSize() / (1024 * 1024), SD.usedBytes() / 1024);
    logMem("sd");

    sqlite3_initialize();
    int rc = sqlite3_open("/sd/k2rfid_test2.db", &db);
    if (rc != SQLITE_OK) { Serial.printf("RESULT: open FAILED rc=%d\n", rc); return; }
    logMem("open");

    exec("PRAGMA page_size=4096");      // only takes effect on a new file
    exec("PRAGMA cache_size=-256");     // 256 KB; 4 KB pages land in PSRAM
    exec("PRAGMA journal_mode=TRUNCATE");
    exec("PRAGMA synchronous=FULL");
    exec("CREATE TABLE IF NOT EXISTS t(seq INTEGER PRIMARY KEY, v TEXT)");

    Serial.println("(a) integrity + state from last boot:");
    // integrity_check is compiled out of Sqlite3Esp32 (SQLITE_OMIT_INTEGRITY_CHECK): a full
    // scan over every row and page stands in. Corruption shows as a non-zero rc here.
    char* ierr = nullptr;
    int irc = sqlite3_exec(db, "SELECT count(*) AS n, max(seq) AS maxseq, sum(length(v)) AS bytes FROM t",
                           printRow, nullptr, &ierr);
    Serial.printf("  scan rc=%d %s\n", irc, ierr ? ierr : "");
    sqlite3_free(ierr);
    sqlite3_exec(db, "PRAGMA page_size", printRow, nullptr, nullptr);

    // (b) single committed insert latency x20
    uint32_t worst = 0, total = 0;
    for (int i = 0; i < 20; i++) {
        uint32_t t0 = millis();
        insertOne("single");
        uint32_t d = millis() - t0;
        total += d; if (d > worst) worst = d;
    }
    Serial.printf("(b) single insert: avg=%u ms worst=%u ms\n", total / 20, worst);

    // (c) 500 inserts in one transaction
    uint32_t t0 = millis();
    exec("BEGIN");
    for (int i = 0; i < 500; i++) insertOne("bulk-row-with-some-text-like-a-spool-name-and-vendor");
    exec("COMMIT");
    Serial.printf("(c) 500 in one txn: %lu ms\n", (unsigned long)(millis() - t0));
    logMem("after-c");

    Serial.println("(e) committed insert every 200 ms; pull power any time");
}

void loop() {
    if (!db) { delay(1000); return; }
    static uint32_t last = 0;
    if (millis() - last < 200) return;
    last = millis();
    if (insertOne("loop")) Serial.printf("seq=%lld\n", (long long)g_seq);
}
