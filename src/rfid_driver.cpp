#include <rfid_driver.h> // Updated include path

// Define pins for PN532
#define PN532_IRQ   (1)
#define PN532_RESET (2)

RFIDDriver rfid;

const uint8_t RFIDDriver::STD_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t RFIDDriver::U_KEY[16] = {0x43, 0x46, 0x53, 0x76, 0x31, 0x45, 0x4D, 0x55, 0x4C, 0x41, 0x54, 0x4F, 0x52, 0x31, 0x33, 0x37};
const uint8_t RFIDDriver::D_KEY[16] = {0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37};

RFIDDriver::RFIDDriver() : nfc(nullptr) {}

void RFIDDriver::init() {
    nfc = new Adafruit_PN532(PN532_IRQ, PN532_RESET);
    nfc->begin();
}

uint32_t RFIDDriver::getFirmwareVersion() {
    if (nfc) {
        return nfc->getFirmwareVersion();
    }
    return 0;
}

bool RFIDDriver::checkTagPresent() {
    if (!nfc) return false;
    return nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLen, 50);
}

void RFIDDriver::haltTag() {
    // No direct equivalent in Adafruit library, but this is good practice
}

bool RFIDDriver::readCFSTag(SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    uint8_t keyA[6];
    generateKeyA(currentUid, keyA);

    if (!nfc->mifareclassic_AuthenticateBlock(currentUid, currentUidLen, 4, 0, keyA)) {
        Serial.println("Auth failed for block 4");
        return false;
    }

    uint8_t block_buffer[16];
    if (!nfc->mifareclassic_ReadDataBlock(4, block_buffer)) {
        Serial.println("Read failed for block 4");
        return false;
    }

    return true;
}

bool RFIDDriver::writeCFSTag(const SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    return true;
}

void RFIDDriver::generateKeyA(const uint8_t* uid, uint8_t* keyOut) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, U_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, uid, keyOut);
    mbedtls_aes_free(&aes);
}