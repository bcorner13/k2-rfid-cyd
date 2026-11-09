#include <spoolman_client.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <config_manager.h>

SpoolmanClient spoolmanClient;

namespace {

constexpr int32_t CONNECT_TIMEOUT_MS = 2000;
constexpr uint16_t READ_TIMEOUT_MS = 3000;
constexpr int MAX_RESPONSE_BYTES = 32 * 1024;   // one spool is ~1-2 KB; refuse anything far larger
constexpr uint32_t WORKER_STACK_BYTES = 8192;

struct LookupRequest {
    char uid[spoolman::UID_BUF];
    char baseUrl[spoolman::BASE_URL_BUF];
};

void runLookup(const LookupRequest& req, SpoolmanResult& res) {
    snprintf(res.uid, sizeof(res.uid), "%s", req.uid);

    char url[spoolman::URL_BUF];
    switch (spoolman::buildLookupUrl(req.baseUrl, req.uid, url, sizeof(url))) {
        case spoolman::UrlResult::Ok:
            break;
        case spoolman::UrlResult::HttpsUnsupported:
            res.state = SpoolmanState::Error;
            snprintf(res.detail, sizeof(res.detail), "https not supported yet");
            return;
        default:
            res.state = SpoolmanState::Error;
            snprintf(res.detail, sizeof(res.detail), "bad URL in settings");
            return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("Spoolman: no WiFi (status %d), not attempting lookup\n", (int)WiFi.status());
        res.state = SpoolmanState::Offline;
        return;
    }

    // Logged before the request: DNS can block here, and only this line would show that.
    Serial.printf("Spoolman: requesting %s (dns %s)\n", url, WiFi.dnsIP().toString().c_str());

    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(CONNECT_TIMEOUT_MS);
    http.setTimeout(READ_TIMEOUT_MS);
    if (!http.begin(url)) {
        res.state = SpoolmanState::Error;
        snprintf(res.detail, sizeof(res.detail), "bad URL in settings");
        return;
    }

    const uint32_t started = millis();
    const int code = http.GET();
    Serial.printf("Spoolman: GET %s -> %d (%lu ms)\n", url, code, (unsigned long)(millis() - started));

    if (code < 0) {     // DNS failure, refused, or timed out: all mean we can't reach it
        http.end();
        res.state = SpoolmanState::Offline;
        return;
    }
    if (code != HTTP_CODE_OK) {
        http.end();
        res.state = SpoolmanState::Error;
        snprintf(res.detail, sizeof(res.detail), "HTTP %d", code);
        return;
    }
    if (http.getSize() > MAX_RESPONSE_BYTES) {
        http.end();
        res.state = SpoolmanState::Error;
        snprintf(res.detail, sizeof(res.detail), "response too large");
        return;
    }

    const String body = http.getString();
    http.end();

    switch (spoolman::parseLookup(body.c_str(), body.length(), req.uid, res.spool)) {
        case spoolman::ParseResult::Found:
            res.state = SpoolmanState::Found;
            break;
        case spoolman::ParseResult::NotFound:
            res.state = SpoolmanState::NotFound;
            break;
        case spoolman::ParseResult::Mismatch:
            res.state = SpoolmanState::Error;
            snprintf(res.detail, sizeof(res.detail), "server returned another spool");
            break;
        case spoolman::ParseResult::Malformed:
            res.state = SpoolmanState::Error;
            snprintf(res.detail, sizeof(res.detail), "unreadable response");
            break;
    }
}

}  // namespace

void SpoolmanClient::workerTask(void* arg) {
    SpoolmanClient* self = static_cast<SpoolmanClient*>(arg);
    LookupRequest req;
    for (;;) {
        if (xQueueReceive(self->requests_, &req, portMAX_DELAY) != pdTRUE) continue;
        SpoolmanResult res;
        runLookup(req, res);
        Serial.printf("Spoolman: %s -> state %d %s (stack free %u)\n", req.uid, (int)res.state, res.detail,
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        xQueueOverwrite(self->results_, &res);
    }
}

void SpoolmanClient::init() {
    // Length 1 + overwrite: only the newest tag matters.
    requests_ = xQueueCreate(1, sizeof(LookupRequest));
    results_ = xQueueCreate(1, sizeof(SpoolmanResult));
    if (!requests_ || !results_ ||
        xTaskCreatePinnedToCore(workerTask, "spoolman", WORKER_STACK_BYTES, this, 1, nullptr, 0) != pdPASS) {
        Serial.println("ERROR: Spoolman client failed to start; lookups disabled");
        requests_ = nullptr;
    }
}

bool SpoolmanClient::lookup(const String& uid) {
    if (!requests_ || config.data.spoolman_url.isEmpty()) {
        Serial.printf("Spoolman: lookup skipped (%s)\n", requests_ ? "URL blank" : "client not running");
        return false;
    }

    LookupRequest req;
    if (!spoolman::normalizeUid(uid.c_str(), req.uid, sizeof(req.uid))) return false;
    if (strcmp(req.uid, requestedUid_) == 0) return false;

    snprintf(req.baseUrl, sizeof(req.baseUrl), "%s", config.data.spoolman_url.c_str());
    snprintf(requestedUid_, sizeof(requestedUid_), "%s", req.uid);
    snprintf(displayUid_, sizeof(displayUid_), "%s", req.uid);
    xQueueOverwrite(requests_, &req);
    return true;
}

bool SpoolmanClient::poll(SpoolmanResult& out) {
    if (!results_) return false;
    SpoolmanResult res;
    if (xQueueReceive(results_, &res, 0) != pdTRUE) return false;
    if (strcmp(res.uid, displayUid_) != 0) return false;   // for a tag no longer on display
    out = res;
    return true;
}

void SpoolmanClient::tagRemoved() {
    requestedUid_[0] = '\0';
}

void SpoolmanClient::clear() {
    requestedUid_[0] = '\0';
    displayUid_[0] = '\0';
}
