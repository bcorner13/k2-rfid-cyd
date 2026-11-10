#include <local_store.h>
#include <ArduinoJson.h>
#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#ifdef ARDUINO
#include <esp_random.h>
#else
#include <random>
#endif

namespace {

constexpr int SCHEMA_VERSION = 1;

const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS filament(id INTEGER PRIMARY KEY, vendor_id INTEGER, vendor TEXT, name TEXT,"
    " material TEXT, color_hex TEXT, multi_colors TEXT, direction TEXT, cfs_id TEXT, weight_g REAL,"
    " nozzle_c INTEGER, bed_c INTEGER);"
    "CREATE TABLE IF NOT EXISTS spool(id INTEGER PRIMARY KEY, filament_id INTEGER, vendor TEXT, name TEXT,"
    " material TEXT, color_hex TEXT, multi_colors TEXT, direction TEXT, cfs_id TEXT, initial_g REAL,"
    " has_remaining INTEGER, remaining_g REAL, location TEXT, archived INTEGER, pending INTEGER DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS tag(uid TEXT PRIMARY KEY, spool_id INTEGER);"
    "CREATE TABLE IF NOT EXISTS pending(seq INTEGER PRIMARY KEY AUTOINCREMENT, kind INTEGER, uid TEXT,"
    " spool_id INTEGER, replaces_spool_id INTEGER DEFAULT 0, remote_spool_id INTEGER DEFAULT 0,"
    " move_tag INTEGER DEFAULT 0, payload TEXT, change_key TEXT, status INTEGER DEFAULT 0,"
    " attempts INTEGER DEFAULT 0, last_error TEXT DEFAULT '');";

// Prepared statement with RAII finalize.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db) {
        if (db && sqlite3_prepare_v2(db, sql, -1, &st_, nullptr) != SQLITE_OK) {
            std::printf("LocalStore: prepare failed: %s -- %.80s\n", sqlite3_errmsg(db), sql);
            st_ = nullptr;
        }
    }
    ~Stmt() { if (st_) sqlite3_finalize(st_); }
    explicit operator bool() const { return st_ != nullptr; }
    Stmt& text(int i, const std::string& v) { sqlite3_bind_text(st_, i, v.c_str(), -1, SQLITE_TRANSIENT); return *this; }
    Stmt& text(int i, const char* v) { sqlite3_bind_text(st_, i, v ? v : "", -1, SQLITE_TRANSIENT); return *this; }
    Stmt& i64(int i, int64_t v) { sqlite3_bind_int64(st_, i, v); return *this; }
    Stmt& real(int i, double v) { sqlite3_bind_double(st_, i, v); return *this; }
    bool step() { return st_ && sqlite3_step(st_) == SQLITE_ROW; }
    bool run() {
        if (!st_) return false;
        const int rc = sqlite3_step(st_);
        if (rc != SQLITE_DONE) std::printf("LocalStore: step failed (%d): %s\n", rc, sqlite3_errmsg(db_));
        return rc == SQLITE_DONE;
    }
    int64_t colI(int c) const { return sqlite3_column_int64(st_, c); }
    double colD(int c) const { return sqlite3_column_double(st_, c); }
    std::string colS(int c) const {
        const unsigned char* t = sqlite3_column_text(st_, c);
        return t ? reinterpret_cast<const char*>(t) : "";
    }
private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* st_ = nullptr;
};

std::string joinColors(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += ','; out += v[i]; }
    return out;
}

std::vector<std::string> splitColors(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.size()) {
        size_t comma = s.find(',', start);
        if (comma == std::string::npos) comma = s.size();
        if (comma > start) out.push_back(s.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

std::string encodeNewSpool(const NewSpool& n) {
    JsonDocument d;
    d["cfs"] = n.cfsId; d["vendor"] = n.vendor; d["name"] = n.name; d["material"] = n.material;
    d["color"] = n.colorHex; d["initial_g"] = n.initialG; d["nozzle_c"] = n.nozzleC; d["bed_c"] = n.bedC;
    std::string out;
    serializeJson(d, out);
    return out;
}

NewSpool decodeNewSpool(const std::string& json) {
    NewSpool n;
    JsonDocument d;
    if (deserializeJson(d, json)) return n;
    n.cfsId = d["cfs"] | ""; n.vendor = d["vendor"] | ""; n.name = d["name"] | "";
    n.material = d["material"] | ""; n.colorHex = d["color"] | "";
    n.initialG = d["initial_g"] | 0.0f; n.nozzleC = d["nozzle_c"] | 0; n.bedC = d["bed_c"] | 0;
    return n;
}

uint32_t randomId() {
#ifdef ARDUINO
    return esp_random();
#else
    std::random_device rd;
    return rd();
#endif
}

// Columns for SpoolRow reads, in readSpoolRow() order.
const char* SPOOL_COLS =
    "s.id, s.filament_id, s.vendor, s.name, s.material, s.color_hex, s.multi_colors, s.direction, s.cfs_id,"
    " s.initial_g, s.has_remaining, s.remaining_g, s.location, s.archived, s.pending,"
    " (SELECT min(uid) FROM tag t WHERE t.spool_id = s.id)";

SpoolRow readSpoolRow(const Stmt& q) {
    SpoolRow r;
    r.id = static_cast<int>(q.colI(0));
    r.filamentId = static_cast<int>(q.colI(1));
    r.vendor = q.colS(2); r.name = q.colS(3); r.material = q.colS(4); r.colorHex = q.colS(5);
    r.multiColors = splitColors(q.colS(6)); r.direction = q.colS(7); r.cfsId = q.colS(8);
    r.initialG = static_cast<float>(q.colD(9));
    r.hasRemaining = q.colI(10) != 0;
    r.remainingG = static_cast<float>(q.colD(11));
    r.location = q.colS(12);
    r.archived = q.colI(13) != 0;
    r.pending = q.colI(14) != 0;
    r.uid = q.colS(15);
    return r;
}

}  // namespace

LocalStore::~LocalStore() { close(); }

bool LocalStore::exec(const char* sql) {
    if (!db_) return false;
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::printf("LocalStore: SQL error %d: %s\n", rc, err ? err : "?");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool LocalStore::open(const char* path) {
    close();
    sqlite3_initialize();   // Sqlite3Esp32 is built with SQLITE_OMIT_AUTOINIT
    if (sqlite3_open(path, &db_) != SQLITE_OK) {
        std::printf("LocalStore: cannot open %s: %s\n", path, db_ ? sqlite3_errmsg(db_) : "?");
        close();
        return false;
    }
    // page_size only applies to a new file. 4 KB pages come from PSRAM on the board; the library's
    // default 1 KB cache made a 500-row transaction take 7 s (plan Task 1).
    if (!exec("PRAGMA page_size=4096; PRAGMA cache_size=-256; PRAGMA journal_mode=TRUNCATE;"
              " PRAGMA synchronous=FULL;") ||
        !exec(SCHEMA_SQL)) {
        close();
        return false;
    }

    {
        Stmt q(db_, "SELECT value FROM meta WHERE key='db_id'");
        if (q.step()) dbId_ = q.colS(0);
    }
    if (dbId_.size() != 8) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned>(randomId()));
        dbId_ = buf;
        char ver[8];
        std::snprintf(ver, sizeof(ver), "%d", SCHEMA_VERSION);
        if (!Stmt(db_, "INSERT OR REPLACE INTO meta(key, value) VALUES('db_id', ?)").text(1, dbId_).run() ||
            !Stmt(db_, "INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version', ?)").text(1, ver).run()) {
            close();
            return false;
        }
    }
    return true;
}

void LocalStore::close() {
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
    dbId_.clear();
}

bool LocalStore::replaceMirror(const std::vector<spoolman::MirrorFilament>& filaments,
                               const std::vector<spoolman::MirrorSpool>& spools, int64_t pulledAtEpoch) {
    if (!db_ || !exec("BEGIN")) return false;
    bool ok = exec("DELETE FROM filament; DELETE FROM spool; DELETE FROM tag;");

    for (size_t i = 0; ok && i < filaments.size(); ++i) {
        const auto& f = filaments[i];
        ok = Stmt(db_, "INSERT OR REPLACE INTO filament(id, vendor_id, vendor, name, material, color_hex,"
                       " multi_colors, direction, cfs_id, weight_g, nozzle_c, bed_c) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)")
                 .i64(1, f.id).i64(2, f.vendorId).text(3, f.vendor).text(4, f.name).text(5, f.material)
                 .text(6, f.colorHex).text(7, joinColors(f.multiColors)).text(8, f.direction).text(9, f.cfsId)
                 .real(10, f.weightG).i64(11, f.nozzleC).i64(12, f.bedC).run();
    }
    // Display fields are copied from the spool's filament; a missing filament leaves them empty.
    // Looked up here, not with INSERT ... SELECT: Sqlite3Esp32 builds with YYSTACKDEPTH 20, and a
    // join with coalesce() overflows its parser ("parser stack overflow", seen on the board).
    std::unordered_map<int, const spoolman::MirrorFilament*> byId;
    for (const auto& f : filaments) byId[f.id] = &f;
    const spoolman::MirrorFilament none;
    for (size_t i = 0; ok && i < spools.size(); ++i) {
        const auto& s = spools[i];
        auto it = byId.find(s.filamentId);
        const spoolman::MirrorFilament& f = it != byId.end() ? *it->second : none;
        ok = Stmt(db_, "INSERT OR REPLACE INTO spool(id, filament_id, vendor, name, material, color_hex, multi_colors,"
                       " direction, cfs_id, initial_g, has_remaining, remaining_g, location, archived, pending)"
                       " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,0)")
                 .i64(1, s.id).i64(2, s.filamentId).text(3, f.vendor).text(4, f.name).text(5, f.material)
                 .text(6, f.colorHex).text(7, joinColors(f.multiColors)).text(8, f.direction).text(9, f.cfsId)
                 .real(10, s.initialG).i64(11, s.hasRemaining ? 1 : 0).real(12, s.remainingG).text(13, s.location)
                 .i64(14, s.archived ? 1 : 0).run();
        for (size_t t = 0; ok && t < s.uids.size(); ++t) {
            ok = Stmt(db_, "INSERT OR REPLACE INTO tag(uid, spool_id) VALUES(?, ?)").text(1, s.uids[t]).i64(2, s.id).run();
        }
    }
    if (ok) {
        char epoch[24];
        std::snprintf(epoch, sizeof(epoch), "%lld", static_cast<long long>(pulledAtEpoch));
        ok = Stmt(db_, "INSERT OR REPLACE INTO meta(key, value) VALUES('last_pull', ?)").text(1, epoch).run();
    }

    // Spoolman doesn't know these changes yet: put them back on top of its copy.
    std::vector<PendingChange> open;
    if (ok) ok = readChanges("status != 2", open);
    for (size_t i = 0; ok && i < open.size(); ++i) ok = applyChange(open[i]);

    if (!ok) {
        exec("ROLLBACK");
        return false;
    }
    return exec("COMMIT");
}

bool LocalStore::listSpools(std::vector<SpoolRow>& out, bool includeArchived) {
    out.clear();
    if (!db_) return false;
    std::string sql = std::string("SELECT ") + SPOOL_COLS + " FROM spool s" +
                      (includeArchived ? "" : " WHERE s.archived = 0") +
                      " ORDER BY s.vendor COLLATE NOCASE, s.name COLLATE NOCASE, s.id";
    Stmt q(db_, sql.c_str());
    if (!q) return false;
    while (q.step()) out.push_back(readSpoolRow(q));
    return true;
}

bool LocalStore::getSpool(int id, SpoolRow& out) {
    std::string sql = std::string("SELECT ") + SPOOL_COLS + " FROM spool s WHERE s.id = ?";
    Stmt q(db_, sql.c_str());
    if (!q) return false;
    q.i64(1, id);
    if (!q.step()) return false;
    out = readSpoolRow(q);
    return true;
}

bool LocalStore::findSpoolByUid(const char* uid, SpoolRow& out) {
    int spoolId = 0;
    {
        Stmt q(db_, "SELECT spool_id FROM tag WHERE uid = ?");
        if (!q) return false;
        q.text(1, uid);
        if (!q.step()) return false;
        spoolId = static_cast<int>(q.colI(0));
    }
    if (!getSpool(spoolId, out)) return false;
    out.uid = uid;   // the tag asked about, even if the spool has several
    return true;
}

bool LocalStore::listFilaments(std::vector<spoolman::MirrorFilament>& out) {
    out.clear();
    Stmt q(db_, "SELECT id, vendor_id, vendor, name, material, color_hex, multi_colors, direction, cfs_id,"
                " weight_g, nozzle_c, bed_c FROM filament ORDER BY id");
    if (!q) return false;
    while (q.step()) {
        spoolman::MirrorFilament f;
        f.id = static_cast<int>(q.colI(0)); f.vendorId = static_cast<int>(q.colI(1));
        f.vendor = q.colS(2); f.name = q.colS(3); f.material = q.colS(4); f.colorHex = q.colS(5);
        f.multiColors = splitColors(q.colS(6)); f.direction = q.colS(7); f.cfsId = q.colS(8);
        f.weightG = static_cast<float>(q.colD(9)); f.nozzleC = static_cast<int>(q.colI(10));
        f.bedC = static_cast<int>(q.colI(11));
        out.push_back(std::move(f));
    }
    return true;
}

int LocalStore::resolveSpoolId(int id) {
    if (id >= 0) return id;
    Stmt q(db_, "SELECT remote_spool_id FROM pending WHERE seq = ? AND kind = 0");
    if (!q) return id;
    q.i64(1, -static_cast<int64_t>(id));
    if (q.step() && q.colI(0) > 0) return static_cast<int>(q.colI(0));
    return id;
}

bool LocalStore::spoolExists(int id) {
    Stmt q(db_, "SELECT 1 FROM spool WHERE id = ?");
    return q && q.i64(1, id).step();
}

bool LocalStore::moveTagTo(const char* uid, int spoolId) {
    return Stmt(db_, "INSERT OR REPLACE INTO tag(uid, spool_id) VALUES(?, ?)").text(1, uid).i64(2, spoolId).run();
}

bool LocalStore::markSpoolPending(int spoolId) {
    return Stmt(db_, "UPDATE spool SET pending = 1 WHERE id = ?").i64(1, spoolId).run();
}

bool LocalStore::applyChange(const PendingChange& c) {
    if (c.kind == ChangeKind::CreateSpool) {
        // Once Spoolman has the spool, put the change on that row instead of a duplicate local one.
        if (c.remoteSpoolId > 0 && spoolExists(c.remoteSpoolId)) {
            return markSpoolPending(c.remoteSpoolId) && moveTagTo(c.uid.c_str(), c.remoteSpoolId);
        }
        const NewSpool& n = c.spool;
        return Stmt(db_, "INSERT OR REPLACE INTO spool(id, filament_id, vendor, name, material, color_hex, multi_colors,"
                         " direction, cfs_id, initial_g, has_remaining, remaining_g, location, archived, pending)"
                         " VALUES(?, 0, ?, ?, ?, ?, '', '', ?, ?, 1, ?, '', 0, 1)")
                   .i64(1, c.spoolId).text(2, n.vendor).text(3, n.name).text(4, n.material).text(5, n.colorHex)
                   .text(6, n.cfsId).real(7, n.initialG).real(8, n.initialG).run() &&
               moveTagTo(c.uid.c_str(), c.spoolId);
    }
    int target = resolveSpoolId(c.spoolId);
    if (!spoolExists(target)) target = c.spoolId;
    return markSpoolPending(target) && moveTagTo(c.uid.c_str(), target);
}

int LocalStore::queueCreateSpool(const NewSpool& spool, const char* uid, int replacesSpoolId) {
    if (!db_ || !exec("BEGIN")) return 0;
    PendingChange c;
    c.kind = ChangeKind::CreateSpool;
    c.uid = uid;
    c.replacesSpoolId = replacesSpoolId;
    c.spool = spool;
    bool ok = Stmt(db_, "INSERT INTO pending(kind, uid, spool_id, replaces_spool_id, payload) VALUES(0, ?, 0, ?, ?)")
                  .text(1, c.uid).i64(2, replacesSpoolId).text(3, encodeNewSpool(spool)).run();
    if (ok) {
        c.seq = sqlite3_last_insert_rowid(db_);
        c.spoolId = -static_cast<int>(c.seq);
        c.changeKey = dbId_ + "-" + std::to_string(c.seq);
        ok = Stmt(db_, "UPDATE pending SET spool_id = ?, change_key = ? WHERE seq = ?")
                 .i64(1, c.spoolId).text(2, c.changeKey).i64(3, c.seq).run() &&
             applyChange(c);
    }
    if (!ok || !exec("COMMIT")) {
        exec("ROLLBACK");
        return 0;
    }
    return c.spoolId;
}

bool LocalStore::queueLinkTag(int spoolId, const char* uid, bool moveTag) {
    if (!db_ || !exec("BEGIN")) return false;
    PendingChange c;
    c.kind = ChangeKind::LinkTag;
    c.uid = uid;
    c.spoolId = spoolId;
    c.moveTag = moveTag;
    bool ok = Stmt(db_, "INSERT INTO pending(kind, uid, spool_id, move_tag) VALUES(1, ?, ?, ?)")
                  .text(1, c.uid).i64(2, spoolId).i64(3, moveTag ? 1 : 0).run();
    if (ok) {
        c.seq = sqlite3_last_insert_rowid(db_);
        c.changeKey = dbId_ + "-" + std::to_string(c.seq);
        ok = Stmt(db_, "UPDATE pending SET change_key = ? WHERE seq = ?").text(1, c.changeKey).i64(2, c.seq).run() &&
             applyChange(c);
    }
    if (!ok || !exec("COMMIT")) {
        exec("ROLLBACK");
        return false;
    }
    return true;
}

bool LocalStore::readChanges(const char* where, std::vector<PendingChange>& out) {
    out.clear();
    std::string sql = std::string("SELECT seq, kind, uid, spool_id, replaces_spool_id, remote_spool_id, move_tag,"
                                  " payload, change_key, status, attempts, last_error FROM pending WHERE ") +
                      where + " ORDER BY seq";
    Stmt q(db_, sql.c_str());
    if (!q) return false;
    while (q.step()) {
        PendingChange c;
        c.seq = q.colI(0);
        c.kind = static_cast<ChangeKind>(q.colI(1));
        c.uid = q.colS(2);
        c.spoolId = static_cast<int>(q.colI(3));
        c.replacesSpoolId = static_cast<int>(q.colI(4));
        c.remoteSpoolId = static_cast<int>(q.colI(5));
        c.moveTag = q.colI(6) != 0;
        if (c.kind == ChangeKind::CreateSpool) c.spool = decodeNewSpool(q.colS(7));
        c.changeKey = q.colS(8);
        c.status = static_cast<ChangeStatus>(q.colI(9));
        c.attempts = static_cast<int>(q.colI(10));
        c.lastError = q.colS(11);
        out.push_back(std::move(c));
    }
    return true;
}

bool LocalStore::pendingChanges(std::vector<PendingChange>& out) {
    if (!db_) { out.clear(); return false; }
    return readChanges("status != 2", out);
}

bool LocalStore::setStatus(int64_t seq, ChangeStatus st, const char* why, bool countAttempt) {
    Stmt q(db_, "UPDATE pending SET status = ?, attempts = attempts + ?, last_error = coalesce(?, last_error)"
                " WHERE seq = ?");
    if (!q) return false;
    q.i64(1, static_cast<int>(st)).i64(2, countAttempt ? 1 : 0);
    if (why) q.text(3, why);
    return q.i64(4, seq).run() && sqlite3_changes(db_) == 1;
}

bool LocalStore::markSent(int64_t seq) { return setStatus(seq, ChangeStatus::Sent, nullptr, true); }
bool LocalStore::markDone(int64_t seq) { return setStatus(seq, ChangeStatus::Done, "", false); }
bool LocalStore::markAttention(int64_t seq, const char* why) {
    return setStatus(seq, ChangeStatus::NeedsAttention, why ? why : "", true);
}

bool LocalStore::setRemoteSpoolId(int64_t seq, int id) {
    return Stmt(db_, "UPDATE pending SET remote_spool_id = ? WHERE seq = ?").i64(1, id).i64(2, seq).run() &&
           sqlite3_changes(db_) == 1;
}

bool LocalStore::discard(int64_t seq) {
    return Stmt(db_, "DELETE FROM pending WHERE seq = ?").i64(1, seq).run() && sqlite3_changes(db_) == 1;
}

int LocalStore::pendingCount() {
    Stmt q(db_, "SELECT count(*) FROM pending WHERE status IN (0, 1)");
    return q.step() ? static_cast<int>(q.colI(0)) : 0;
}

int LocalStore::attentionCount() {
    Stmt q(db_, "SELECT count(*) FROM pending WHERE status = 3");
    return q.step() ? static_cast<int>(q.colI(0)) : 0;
}

int64_t LocalStore::lastPullEpoch() {
    Stmt q(db_, "SELECT value FROM meta WHERE key = 'last_pull'");
    return q.step() ? std::strtoll(q.colS(0).c_str(), nullptr, 10) : 0;
}
