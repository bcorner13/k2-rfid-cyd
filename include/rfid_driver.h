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

class RFIDDriver {
public:
    RFIDDriver();
    void init();
    uint32_t getFirmwareVersion();
    bool checkTagPresent();
    void haltTag();

    // K2 / CFS Operations
    bool readCFSTag(SpoolData& spoolData);
    bool writeCFSTag(const SpoolData& spoolData);

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

    // Expose UID for external use (tag ↔ inventory reconciliation)
    const uint8_t* getUID() const { return currentUid; }
    uint8_t getUIDLength() const { return currentUidLen; }

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