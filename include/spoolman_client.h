#pragma once
/**
 * @file spoolman_client.h
 * @brief Non-blocking, read-only Spoolman lookup by tag UID.
 *
 * HTTP runs on its own FreeRTOS task because a dead DNS server or an unreachable host can block
 * for seconds, and the main loop drives LVGL and RFID. The worker never touches LVGL, RFID, or
 * config: requests carry their own copy of the URL, and results are polled from loop().
 *
 * Tag read and write never depend on this. Offline only changes what the Spoolman line shows.
 */
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <spoolman_parse.h>

enum class SpoolmanState : uint8_t { Found, NotFound, Offline, Error };

struct SpoolmanResult {
    char uid[spoolman::UID_BUF] = "";
    SpoolmanState state = SpoolmanState::Error;
    spoolman::SpoolInfo spool;
    char detail[48] = "";   // short reason, for Error
};

class SpoolmanClient {
public:
    void init();

    // Queues a lookup unless this tag was already requested. True when a new lookup started.
    bool lookup(const String& uid);

    // Non-blocking. True when a result for the tag on display is ready.
    bool poll(SpoolmanResult& out);

    // Tag lifted: the next placement looks it up again, but a lookup in flight still reports.
    void tagRemoved();

    // Dashboard no longer shows this tag: also drop any result still in flight.
    void clear();

private:
    static void workerTask(void* arg);

    QueueHandle_t requests_{nullptr};
    QueueHandle_t results_{nullptr};
    char requestedUid_[spoolman::UID_BUF] = "";
    char displayUid_[spoolman::UID_BUF] = "";
};

extern SpoolmanClient spoolmanClient;
