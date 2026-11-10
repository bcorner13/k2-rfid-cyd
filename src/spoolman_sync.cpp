#include <spoolman_sync.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <config_manager.h>
#include <sd_storage.h>
#include <spoolman_mirror_parse.h>
#include <time.h>

SpoolmanSync spoolmanSync;

namespace {

constexpr int32_t CONNECT_TIMEOUT_MS = 2000;   // same as SpoolmanClient
constexpr uint16_t READ_TIMEOUT_MS = 3000;
constexpr int MAX_PAGE_BYTES = 64 * 1024;
constexpr int PAGE_LIMIT = 50;
constexpr int MAX_OFFSET = 5000;               // 100 pages: far beyond a home inventory
constexpr uint32_t TASK_STACK_BYTES = 12288;

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
char s_baseUrl[spoolman::BASE_URL_BUF] = "";
SyncState s_state = SyncState::NoStore;
int64_t s_lastPullEpoch = 0;
uint32_t s_lastPullMillis = 0;
uint32_t s_changeCounter = 0;
uint32_t s_failures = 0;

void setState(SyncState st) {
    portENTER_CRITICAL(&s_mux);
    s_state = st;
    portEXIT_CRITICAL(&s_mux);
}

enum class Fetch : uint8_t { Ok, Unreachable, HttpError, BadData };

const char* fetchText(Fetch f) {
    switch (f) {
        case Fetch::Ok: return "ok";
        case Fetch::Unreachable: return "unreachable";
        case Fetch::HttpError: return "HTTP error";
        case Fetch::BadData: return "bad data";
    }
    return "?";
}

Fetch httpGet(const char* url, String& body) {
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(CONNECT_TIMEOUT_MS);
    http.setTimeout(READ_TIMEOUT_MS);
    if (!http.begin(url)) return Fetch::HttpError;
    const int code = http.GET();
    if (code < 0) {        // DNS failure, refused, timeout: all mean "can't reach it"
        http.end();
        return Fetch::Unreachable;
    }
    if (code != HTTP_CODE_OK) {
        Serial.printf("Sync: GET %s -> %d\n", url, code);
        http.end();
        return code >= 500 ? Fetch::Unreachable : Fetch::HttpError;
    }
    if (http.getSize() > MAX_PAGE_BYTES) {
        http.end();
        return Fetch::BadData;
    }
    body = http.getString();
    http.end();
    return (int)body.length() > MAX_PAGE_BYTES ? Fetch::BadData : Fetch::Ok;
}

// Every page of one list endpoint, appended to `out`. Any failure leaves the caller's mirror alone.
template <typename Row, typename ParseFn>
Fetch fetchAll(const char* base, const char* pathPrefix, ParseFn parse, std::vector<Row>& out) {
    for (int offset = 0; offset <= MAX_OFFSET; offset += PAGE_LIMIT) {
        char path[96];
        snprintf(path, sizeof(path), "%slimit=%d&offset=%d", pathPrefix, PAGE_LIMIT, offset);
        char url[spoolman::URL_BUF];
        if (spoolman::buildApiUrl(base, path, url, sizeof(url)) != spoolman::UrlResult::Ok) return Fetch::HttpError;
        String body;
        const Fetch f = httpGet(url, body);
        if (f != Fetch::Ok) return f;
        size_t count = 0;
        if (parse(body.c_str(), body.length(), out, count) != spoolman::PageResult::Ok) {
            Serial.printf("Sync: unreadable page %s\n", url);
            return Fetch::BadData;
        }
        if (count < (size_t)PAGE_LIMIT) return Fetch::Ok;
    }
    return Fetch::BadData;   // more pages than any home inventory: refuse rather than half-pull
}

int64_t clockEpoch() {
    const time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year + 1900 >= 2026 ? (int64_t)now : PULL_EPOCH_UNKNOWN;
}

}  // namespace

void SpoolmanSync::init() {
    setBaseUrl(config.data.spoolman_url.c_str());
    if (sdStorage.available()) {
        StoreGuard g(portMAX_DELAY);
        if (g) {
            s_lastPullEpoch = g->lastPullEpoch();
            setState(s_lastPullEpoch ? SyncState::Offline : SyncState::NeverSynced);
        }
    }
    if (xTaskCreatePinnedToCore(taskMain, "spoolsync", TASK_STACK_BYTES, this, 1, &task_, 0) != pdPASS) {
        Serial.println("ERROR: sync task failed to start; local copy won't update");
        task_ = nullptr;
    }
}

void SpoolmanSync::setBaseUrl(const char* url) {
    portENTER_CRITICAL(&s_mux);
    snprintf(s_baseUrl, sizeof(s_baseUrl), "%s", url ? url : "");
    portEXIT_CRITICAL(&s_mux);
}

void SpoolmanSync::kick() {
    if (task_) xTaskNotifyGive(task_);
}

SyncState SpoolmanSync::state() const {
    portENTER_CRITICAL(&s_mux);
    SyncState st = s_state;
    portEXIT_CRITICAL(&s_mux);
    return st;
}

int64_t SpoolmanSync::lastPullEpoch() const {
    portENTER_CRITICAL(&s_mux);
    int64_t v = s_lastPullEpoch;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint32_t SpoolmanSync::lastPullMillis() const {
    portENTER_CRITICAL(&s_mux);
    uint32_t v = s_lastPullMillis;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint32_t SpoolmanSync::changeCounter() const {
    portENTER_CRITICAL(&s_mux);
    uint32_t v = s_changeCounter;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

void SpoolmanSync::taskMain(void* arg) {
    SpoolmanSync* self = static_cast<SpoolmanSync*>(arg);
    vTaskDelay(pdMS_TO_TICKS(3000));   // let Wi-Fi come up after boot
    for (;;) {
        self->runOnce();
        const SyncState st = self->state();
        const uint32_t wait = st == SyncState::NoStore ? 600000 : nextSyncDelayMs(st, s_failures);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait));
    }
}

void SpoolmanSync::runOnce() {
    if (!sdStorage.available()) {
        setState(SyncState::NoStore);
        return;
    }
    char base[spoolman::BASE_URL_BUF];
    portENTER_CRITICAL(&s_mux);
    snprintf(base, sizeof(base), "%s", s_baseUrl);
    const SyncState before = s_state;
    portEXIT_CRITICAL(&s_mux);

    if (base[0] == '\0' || WiFi.status() != WL_CONNECTED) {
        ++s_failures;
        setState(lastPullEpoch() ? SyncState::Offline : SyncState::NeverSynced);
        return;
    }

    setState(SyncState::Syncing);
    Serial.println("Sync: pulling");
    const uint32_t t0 = millis();
    std::vector<spoolman::MirrorFilament> filaments;
    std::vector<spoolman::MirrorSpool> spools;
    Fetch f = fetchAll(base, "/filament?", spoolman::parseFilamentPage, filaments);
    if (f == Fetch::Ok) f = fetchAll(base, "/spool?allow_archived=true&", spoolman::parseSpoolPage, spools);

    if (f != Fetch::Ok) {
        ++s_failures;
        Serial.printf("Sync: pull failed (%s), attempt %u; local copy unchanged\n", fetchText(f), (unsigned)s_failures);
        setState(lastPullEpoch() ? SyncState::Offline : SyncState::NeverSynced);
        return;
    }

    Serial.printf("Sync: fetched %u filaments, %u spools in %lu ms; writing local copy\n",
                  (unsigned)filaments.size(), (unsigned)spools.size(), (unsigned long)(millis() - t0));
    const int64_t epoch = clockEpoch();
    bool stored = false;
    {
        StoreGuard g(portMAX_DELAY);
        stored = g && g->replaceMirror(filaments, spools, epoch);
    }
    if (!stored) {
        sdStorage.markFailed("pull write failed");
        setState(SyncState::NoStore);
        return;
    }
    s_failures = 0;
    portENTER_CRITICAL(&s_mux);
    s_lastPullEpoch = epoch;
    s_lastPullMillis = millis();
    ++s_changeCounter;
    s_state = SyncState::Online;
    portEXIT_CRITICAL(&s_mux);
    Serial.printf("Sync: pulled %u filaments, %u spools in %lu ms (was state %d, internal heap %u, stack free %u)\n",
                  (unsigned)filaments.size(), (unsigned)spools.size(), (unsigned long)(millis() - t0), (int)before,
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}
