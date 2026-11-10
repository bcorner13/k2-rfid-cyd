#pragma once
/**
 * @file local_store.h
 * @brief SQLite local copy of Spoolman plus the device's pending changes.
 *
 * Spoolman is the master. replaceMirror() swaps in a full pull in one transaction, then reapplies
 * every change not yet confirmed by Spoolman, so a pull never hides an unsynced tag write.
 * The device's own changes (create spool, link tag) are recorded here and applied to the copy at once.
 *
 * Plain SQLite C API, no Arduino: the same file is host-tested against the system libsqlite3
 * (pio test -e native) and runs on the board through Sqlite3Esp32. Not thread-safe: on the board
 * every call goes through SdStorage's lock.
 * Design: docs/superpowers/specs/2026-11-09-offline-inventory-design.md
 */
#include <cstdint>
#include <string>
#include <vector>
#include <spoolman_mirror_parse.h>

struct sqlite3;

struct SpoolRow {
    int id = 0;                    // < 0 = created on this device, not yet in Spoolman
    int filamentId = 0;
    std::string vendor, name, material, colorHex, direction, cfsId, location, uid;
    std::vector<std::string> multiColors;
    float initialG = 0.0f;
    bool hasRemaining = false;
    float remainingG = 0.0f;
    bool archived = false;
    bool pending = false;          // an unsynced change touches this spool
};

struct NewSpool {
    std::string cfsId, vendor, name, material, colorHex;
    float initialG = 0.0f;
    int nozzleC = 0;
    int bedC = 0;
};

enum class ChangeKind : uint8_t { CreateSpool = 0, LinkTag = 1 };
enum class ChangeStatus : uint8_t { Pending = 0, Sent = 1, Done = 2, NeedsAttention = 3 };

struct PendingChange {
    int64_t seq = 0;
    ChangeKind kind = ChangeKind::LinkTag;
    std::string uid, changeKey, lastError;
    int spoolId = 0;               // LinkTag target (may be < 0); CreateSpool: its local id
    int replacesSpoolId = 0;       // CreateSpool via "Replace spool #N", else 0
    int remoteSpoolId = 0;         // set once Spoolman returned an id
    bool moveTag = false;          // LinkTag: Bradley chose "Move tag"
    NewSpool spool;
    ChangeStatus status = ChangeStatus::Pending;
    int attempts = 0;
};

class LocalStore {
public:
    LocalStore() = default;
    ~LocalStore();
    LocalStore(const LocalStore&) = delete;
    LocalStore& operator=(const LocalStore&) = delete;

    bool open(const char* path);   // creates schema v1; page_size 4096, cache -256, TRUNCATE, FULL
    void close();
    bool isOpen() const { return db_ != nullptr; }
    const std::string& dbId() const { return dbId_; }   // 8 hex chars, random, fixed per database

    bool replaceMirror(const std::vector<spoolman::MirrorFilament>& filaments,
                       const std::vector<spoolman::MirrorSpool>& spools, int64_t pulledAtEpoch);
    bool listSpools(std::vector<SpoolRow>& out, bool includeArchived);   // ordered vendor, name, id
    bool getSpool(int id, SpoolRow& out);
    bool findSpoolByUid(const char* uid, SpoolRow& out);
    bool listFilaments(std::vector<spoolman::MirrorFilament>& out);

    int  queueCreateSpool(const NewSpool& spool, const char* uid, int replacesSpoolId);  // local id (<0), 0 on error
    bool queueLinkTag(int spoolId, const char* uid, bool moveTag);
    bool pendingChanges(std::vector<PendingChange>& out);                // non-Done, seq order
    bool markSent(int64_t seq);                                          // status Sent, attempts + 1
    bool setRemoteSpoolId(int64_t seq, int id);
    bool markDone(int64_t seq);
    bool markAttention(int64_t seq, const char* why);                   // attempts + 1
    bool discard(int64_t seq);
    int  pendingCount();      // Pending + Sent
    int  attentionCount();    // NeedsAttention
    int64_t lastPullEpoch();  // 0 = never

    /// Spoolman id for a spool id: a local id (<0) maps to its create's remote id once known, else unchanged.
    int resolveSpoolId(int id);

private:
    bool exec(const char* sql);
    bool applyChange(const PendingChange& c);
    bool moveTagTo(const char* uid, int spoolId);
    bool markSpoolPending(int spoolId);
    bool spoolExists(int id);
    bool readChanges(const char* where, std::vector<PendingChange>& out);
    bool setStatus(int64_t seq, ChangeStatus st, const char* why, bool countAttempt);

    sqlite3* db_ = nullptr;
    std::string dbId_;
};
