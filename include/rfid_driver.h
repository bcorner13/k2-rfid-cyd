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

    // State
    uint8_t currentUid[7];
    uint8_t currentUidLen;
};

extern RFIDDriver rfid;