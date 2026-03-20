#pragma once
/**
 * @file rfid_driver.h
 * @brief PN532 NFC driver for MIFARE Classic 1K CFS tags (read/write).
 *
 * Uses AES-derived Key A for sector auth; readCFSTag/writeCFSTag operate
 * on the CFS payload. SpoolData holds the decoded/encoded string format.
 */

#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "mbedtls/aes.h"
#include "spool_data.h"
#include "k2_tag.h"
#include "tag_data.h"

// Mirror voting result for sectors 6-8
struct MirrorResult {
    RemainingBlock data;        // Resolved remaining filament data
    uint8_t agreement;          // 3 = unanimous, 2 = majority, 0 = all differ
    int8_t  badMirror;          // Index of disagreeing mirror (0-2), -1 if unanimous or all differ
    bool    valid;              // true if data is usable (agreement >= 2)
};

// P0.6: Weight reconciliation result
static constexpr uint32_t WEIGHT_TOLERANCE_G = 5;  // ±5g tolerance

struct WeightReconcileResult {
    uint32_t tag_weight_g;
    uint32_t inventory_weight_g;
    int32_t  delta_g;           // tag - inventory (signed)
    bool     in_sync;           // abs(delta) <= WEIGHT_TOLERANCE_G
};

// P0.7: Tag version info
struct TagVersionInfo {
    uint8_t  version;           // TAG_VERSION_V1 or TAG_VERSION_V2, 0 on read failure
    bool     is_v2;
    bool     is_our_v2;         // true if origin_magic == K2FX_MAGIC (we wrote it)
    uint32_t origin_magic;      // raw magic from sector 10, 0 if not v2
};

class RFIDDriver {
public:
    RFIDDriver();
    void init();
    uint32_t getFirmwareVersion();
    bool checkTagPresent();
    void haltTag();

    // K2 / CFS Operations (legacy SpoolData API)
    bool readCFSTag(SpoolData& spoolData);
    bool writeCFSTag(const SpoolData& spoolData);

    // P1.1: TagData-based read/write (FSD Section 7.9)
    bool readTag(TagData& out);
    bool writeTag(const TagData& in);

    // CRC-32 (IEEE 802.3 / zlib-compatible)
    static uint32_t computeCRC32(const uint8_t* data, size_t length);

    /** Read data blocks from a range of sectors into a flat buffer.
     *  Reads 3 data blocks per sector (skips sector trailers).
     *  Buffer must be at least (lastSector - firstSector + 1) * 48 bytes.
     *  Returns number of bytes read, or 0 on failure. */
    size_t readSectorRange(uint8_t firstSector, uint8_t lastSector, uint8_t* outBuf);

    /** Read CRC32 from sector 15 control block. Returns 0 on failure. */
    uint32_t readTagCRC32();

    /** Validate tag CRC. Reads sectors per version, computes CRC, compares to stored.
     *  Sets computedCRC and storedCRC output params. Returns true if match. */
    bool validateTagCRC(uint8_t tagVersion, uint32_t& computedCRC, uint32_t& storedCRC);

    /** Write CRC32 to sector 15 control block. */
    bool writeTagCRC32(uint32_t crc);

    /** Read sectors 6-8 and perform majority voting per FSD 7.8.
     *  Returns MirrorResult with resolved data and agreement status. */
    MirrorResult readMirrors();

    /** Write RemainingBlock to all 3 mirror sectors (6→7→8) with read-back verify. */
    bool writeMirrors(const RemainingBlock& data);

    // Expose UID for external use (tag ↔ inventory reconciliation)
    const uint8_t* getUID() const { return currentUid; }
    uint8_t getUIDLength() const { return currentUidLen; }

    // P0.6: Format UID as colon-separated hex string (e.g. "04:A3:2B:1C")
    String formatUID() const;

    // P0.6: Compare tag weight vs inventory weight with ±5g tolerance
    static WeightReconcileResult reconcileWeight(uint32_t tag_weight_g, uint32_t inventory_weight_g);

    // P0.7: Read tag version from sector 1 format block
    uint8_t readTagVersion();

    // P0.7: Read version + check v2 origin magic from sector 10
    TagVersionInfo readTagVersionInfo();

    // Get last CFS raw dump string (full hex log, shown on RFID Raw screen)
    const char* getLastRawDump() const;

    // Get last decoded CFS payload string (40-char uppercase ASCII, shown in table)
    const char* getLastPayload() const;

    // Get mirror result from the last readCFSTag() call (sectors 6-8, remaining filament)
    const MirrorResult& getLastMirrors() const;

private:
    Adafruit_PN532* nfc;

    // Keys
    static const uint8_t STD_KEY[6];
    static const uint8_t U_KEY[16];
    static const uint8_t D_KEY[16];

    // Helpers
    void generateKeyA(const uint8_t* uid, uint8_t* keyOut);
    bool decryptBlock(const uint8_t* input, uint8_t* output);
    bool encryptBlock(const uint8_t* input, uint8_t* output);
    bool authenticateSector(uint8_t sector);

    // State
    uint8_t currentUid[7];
    uint8_t currentUidLen;
    uint8_t cachedKeyA[6];
    bool keyACached;
};

extern RFIDDriver rfid;