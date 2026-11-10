// Host tests for LocalStore (SQLite local copy of Spoolman + pending changes): pio test -e native
// Runs against the Mac's system libsqlite3; the board uses the same C API via Sqlite3Esp32.
#include <unity.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "local_store.h"

using spoolman::MirrorFilament;
using spoolman::MirrorSpool;

static const char* TMP_DB = "test_local_store_tmp.db";

void setUp() {}
void tearDown() { std::remove(TMP_DB); }

static MirrorFilament fil(int id, const char* vendor, const char* name, const char* color, const char* cfs = "") {
    MirrorFilament f;
    f.id = id; f.vendorId = 1; f.vendor = vendor; f.name = name; f.material = "PLA";
    f.colorHex = color; f.cfsId = cfs; f.weightG = 1000;
    return f;
}

static MirrorSpool spl(int id, int filamentId, const char* uid = nullptr, bool archived = false) {
    MirrorSpool s;
    s.id = id; s.filamentId = filamentId; s.initialG = 1000; s.hasRemaining = true; s.remainingG = 742;
    s.location = "K2 CFS"; s.archived = archived;
    if (uid) s.uids.push_back(uid);
    return s;
}

// 2 filaments, spools 5 (tag 04A2B3C4), 7 (no tag), 9 (archived).
static void pullBasic(LocalStore& st, bool tagOn7 = false) {
    std::vector<MirrorFilament> f = { fil(1, "Creality", "Ender-PLA", "E53935", "08001"),
                                      fil(2, "Polymaker", "Panchroma PLA", "000000") };
    std::vector<MirrorSpool> s = { spl(5, 1, "04A2B3C4"), spl(7, 2, tagOn7 ? "11223344" : nullptr), spl(9, 2, nullptr, true) };
    TEST_ASSERT_TRUE(st.replaceMirror(f, s, 1762700000));
}

static NewSpool newSpool() {
    NewSpool n;
    n.cfsId = "08001"; n.vendor = "Creality"; n.name = "Ender-PLA"; n.material = "PLA";
    n.colorHex = "E53935"; n.initialG = 1000; n.nozzleC = 210; n.bedC = 60;
    return n;
}

void test_open_creates_schema_and_db_id() {
    std::remove(TMP_DB);
    std::string id1;
    {
        LocalStore st;
        TEST_ASSERT_TRUE(st.open(TMP_DB));
        TEST_ASSERT_TRUE(st.isOpen());
        id1 = st.dbId();
        TEST_ASSERT_EQUAL_UINT(8, id1.size());
        TEST_ASSERT_EQUAL(0, (int)st.lastPullEpoch());
    }
    LocalStore st2;
    TEST_ASSERT_TRUE(st2.open(TMP_DB));
    TEST_ASSERT_EQUAL_STRING(id1.c_str(), st2.dbId().c_str());
}

void test_replace_mirror_lists_spools() {
    LocalStore st; TEST_ASSERT_TRUE(st.open(":memory:"));
    pullBasic(st);
    std::vector<SpoolRow> rows;
    TEST_ASSERT_TRUE(st.listSpools(rows, false));
    TEST_ASSERT_EQUAL_UINT(2, rows.size());
    // ordered vendor, name, id: Creality before Polymaker
    TEST_ASSERT_EQUAL(5, rows[0].id);
    TEST_ASSERT_EQUAL_STRING("Creality", rows[0].vendor.c_str());
    TEST_ASSERT_EQUAL_STRING("Ender-PLA", rows[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("E53935", rows[0].colorHex.c_str());
    TEST_ASSERT_EQUAL_STRING("08001", rows[0].cfsId.c_str());
    TEST_ASSERT_EQUAL_STRING("04A2B3C4", rows[0].uid.c_str());
    TEST_ASSERT_EQUAL_FLOAT(742, rows[0].remainingG);
    TEST_ASSERT_FALSE(rows[0].pending);
    TEST_ASSERT_EQUAL(7, rows[1].id);
    TEST_ASSERT_EQUAL_STRING("", rows[1].uid.c_str());
    TEST_ASSERT_TRUE(st.listSpools(rows, true));
    TEST_ASSERT_EQUAL_UINT(3, rows.size());
    TEST_ASSERT_EQUAL(1762700000, (int)st.lastPullEpoch());
}

void test_find_by_uid() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    SpoolRow r;
    TEST_ASSERT_TRUE(st.findSpoolByUid("04A2B3C4", r));
    TEST_ASSERT_EQUAL(5, r.id);
    TEST_ASSERT_FALSE(st.findSpoolByUid("DEADBEEF", r));
    TEST_ASSERT_TRUE(st.getSpool(7, r));
    TEST_ASSERT_EQUAL_STRING("Panchroma PLA", r.name.c_str());
    std::vector<MirrorFilament> fs;
    TEST_ASSERT_TRUE(st.listFilaments(fs));
    TEST_ASSERT_EQUAL_UINT(2, fs.size());
}

void test_queue_create_visible_immediately() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    int id = st.queueCreateSpool(newSpool(), "AABBCCDD", 0);
    TEST_ASSERT_EQUAL(-1, id);
    SpoolRow r;
    TEST_ASSERT_TRUE(st.findSpoolByUid("AABBCCDD", r));
    TEST_ASSERT_EQUAL(-1, r.id);
    TEST_ASSERT_TRUE(r.pending);
    TEST_ASSERT_EQUAL_STRING("Ender-PLA", r.name.c_str());
    TEST_ASSERT_EQUAL_FLOAT(1000, r.initialG);
    std::vector<SpoolRow> rows;
    st.listSpools(rows, false);
    TEST_ASSERT_EQUAL_UINT(3, rows.size());
    TEST_ASSERT_EQUAL(1, st.pendingCount());
    std::vector<PendingChange> pc;
    TEST_ASSERT_TRUE(st.pendingChanges(pc));
    TEST_ASSERT_EQUAL_UINT(1, pc.size());
    TEST_ASSERT_EQUAL((int)ChangeKind::CreateSpool, (int)pc[0].kind);
    TEST_ASSERT_EQUAL_STRING((st.dbId() + "-1").c_str(), pc[0].changeKey.c_str());
    TEST_ASSERT_EQUAL_STRING("Creality", pc[0].spool.vendor.c_str());
    TEST_ASSERT_EQUAL(210, pc[0].spool.nozzleC);
}

void test_queue_link_moves_tag() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    TEST_ASSERT_TRUE(st.queueLinkTag(7, "04A2B3C4", false));
    SpoolRow r;
    TEST_ASSERT_TRUE(st.findSpoolByUid("04A2B3C4", r));
    TEST_ASSERT_EQUAL(7, r.id);
    TEST_ASSERT_TRUE(r.pending);
    TEST_ASSERT_TRUE(st.getSpool(5, r));
    TEST_ASSERT_EQUAL_STRING("", r.uid.c_str());
}

void test_replace_mirror_reapplies_pending_link() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    st.queueLinkTag(7, "04A2B3C4", true);
    pullBasic(st);   // Spoolman still has the tag on 5
    SpoolRow r;
    TEST_ASSERT_TRUE(st.findSpoolByUid("04A2B3C4", r));
    TEST_ASSERT_EQUAL(7, r.id);
    TEST_ASSERT_TRUE(r.pending);
    std::vector<PendingChange> pc;
    st.pendingChanges(pc);
    TEST_ASSERT_TRUE(pc[0].moveTag);
}

void test_replace_mirror_keeps_local_spool() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    st.queueCreateSpool(newSpool(), "AABBCCDD", 0);
    pullBasic(st);
    SpoolRow r;
    TEST_ASSERT_TRUE(st.getSpool(-1, r));
    TEST_ASSERT_EQUAL_STRING("AABBCCDD", r.uid.c_str());
}

void test_reapply_uses_remote_id_once_known() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    st.queueCreateSpool(newSpool(), "AABBCCDD", 0);
    st.queueLinkTag(-1, "55667788", false);   // a second tag for the new spool
    std::vector<PendingChange> pc; st.pendingChanges(pc);
    st.markSent(pc[0].seq);
    st.setRemoteSpoolId(pc[0].seq, 12);
    // Spoolman now has spool 12 (created, tags not yet linked)
    std::vector<MirrorFilament> f = { fil(1, "Creality", "Ender-PLA", "E53935", "08001") };
    std::vector<MirrorSpool> s = { spl(5, 1, "04A2B3C4"), spl(12, 1) };
    st.replaceMirror(f, s, 1762700100);
    SpoolRow r;
    TEST_ASSERT_FALSE(st.getSpool(-1, r));                 // no duplicate local row
    TEST_ASSERT_TRUE(st.findSpoolByUid("AABBCCDD", r));
    TEST_ASSERT_EQUAL(12, r.id);
    TEST_ASSERT_TRUE(r.pending);
    TEST_ASSERT_TRUE(st.findSpoolByUid("55667788", r));
    TEST_ASSERT_EQUAL(12, r.id);
    TEST_ASSERT_EQUAL(12, st.resolveSpoolId(-1));
    TEST_ASSERT_EQUAL(7, st.resolveSpoolId(7));
}

void test_done_change_not_reapplied() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    st.queueLinkTag(7, "04A2B3C4", false);
    std::vector<PendingChange> pc; st.pendingChanges(pc);
    TEST_ASSERT_TRUE(st.markDone(pc[0].seq));
    pullBasic(st);   // Spoolman says the tag is on 5 (it wins)
    SpoolRow r;
    st.findSpoolByUid("04A2B3C4", r);
    TEST_ASSERT_EQUAL(5, r.id);
    TEST_ASSERT_FALSE(r.pending);
    TEST_ASSERT_EQUAL(0, st.pendingCount());
    st.pendingChanges(pc);
    TEST_ASSERT_EQUAL_UINT(0, pc.size());
}

void test_failed_pull_leaves_mirror() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    // A spool whose filament isn't in the pull still lands, with empty display fields.
    std::vector<MirrorFilament> f = { fil(1, "Creality", "Ender-PLA", "E53935") };
    std::vector<MirrorSpool> s = { spl(5, 1, "04A2B3C4"), spl(8, 99) };
    TEST_ASSERT_TRUE(st.replaceMirror(f, s, 1762700200));
    SpoolRow r;
    TEST_ASSERT_TRUE(st.getSpool(8, r));
    TEST_ASSERT_EQUAL_STRING("", r.name.c_str());
    TEST_ASSERT_FALSE(st.getSpool(7, r));   // fully replaced, not merged
    TEST_ASSERT_EQUAL(1762700200, (int)st.lastPullEpoch());
}

void test_mark_sent_survives_reopen() {
    std::remove(TMP_DB);
    {
        LocalStore st; TEST_ASSERT_TRUE(st.open(TMP_DB));
        pullBasic(st);
        st.queueCreateSpool(newSpool(), "AABBCCDD", 5);
        std::vector<PendingChange> pc; st.pendingChanges(pc);
        TEST_ASSERT_TRUE(st.markSent(pc[0].seq));
    }
    LocalStore st; TEST_ASSERT_TRUE(st.open(TMP_DB));
    std::vector<PendingChange> pc; st.pendingChanges(pc);
    TEST_ASSERT_EQUAL_UINT(1, pc.size());
    TEST_ASSERT_EQUAL((int)ChangeStatus::Sent, (int)pc[0].status);
    TEST_ASSERT_EQUAL(1, pc[0].attempts);
    TEST_ASSERT_EQUAL(5, pc[0].replacesSpoolId);
    TEST_ASSERT_EQUAL(0, pc[0].remoteSpoolId);
    SpoolRow r;
    TEST_ASSERT_TRUE(st.findSpoolByUid("AABBCCDD", r));
    TEST_ASSERT_EQUAL(-1, r.id);
}

void test_attention_and_discard() {
    LocalStore st; st.open(":memory:");
    pullBasic(st);
    st.queueLinkTag(7, "04A2B3C4", false);
    std::vector<PendingChange> pc; st.pendingChanges(pc);
    TEST_ASSERT_TRUE(st.markAttention(pc[0].seq, "Tag is on another spool in Spoolman"));
    TEST_ASSERT_EQUAL(1, st.attentionCount());
    TEST_ASSERT_EQUAL(0, st.pendingCount());
    st.pendingChanges(pc);
    TEST_ASSERT_EQUAL_UINT(1, pc.size());   // still retried every sync
    TEST_ASSERT_EQUAL((int)ChangeStatus::NeedsAttention, (int)pc[0].status);
    TEST_ASSERT_EQUAL_STRING("Tag is on another spool in Spoolman", pc[0].lastError.c_str());
    TEST_ASSERT_TRUE(st.discard(pc[0].seq));
    TEST_ASSERT_EQUAL(0, st.attentionCount());
    pullBasic(st);
    SpoolRow r;
    st.findSpoolByUid("04A2B3C4", r);
    TEST_ASSERT_EQUAL(5, r.id);
}

void test_not_open_fails_safely() {
    LocalStore st;
    std::vector<SpoolRow> rows;
    SpoolRow r;
    TEST_ASSERT_FALSE(st.isOpen());
    TEST_ASSERT_FALSE(st.listSpools(rows, false));
    TEST_ASSERT_FALSE(st.findSpoolByUid("04A2B3C4", r));
    TEST_ASSERT_EQUAL(0, st.queueCreateSpool(newSpool(), "AABBCCDD", 0));
    TEST_ASSERT_FALSE(st.queueLinkTag(5, "04A2B3C4", false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_open_creates_schema_and_db_id);
    RUN_TEST(test_replace_mirror_lists_spools);
    RUN_TEST(test_find_by_uid);
    RUN_TEST(test_queue_create_visible_immediately);
    RUN_TEST(test_queue_link_moves_tag);
    RUN_TEST(test_replace_mirror_reapplies_pending_link);
    RUN_TEST(test_replace_mirror_keeps_local_spool);
    RUN_TEST(test_reapply_uses_remote_id_once_known);
    RUN_TEST(test_done_change_not_reapplied);
    RUN_TEST(test_failed_pull_leaves_mirror);
    RUN_TEST(test_mark_sent_survives_reopen);
    RUN_TEST(test_attention_and_discard);
    RUN_TEST(test_not_open_fails_safely);
    return UNITY_END();
}
