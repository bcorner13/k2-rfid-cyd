#include <rfid_driver.h>
#include <esp_rom_crc.h>

// Define pins for PN532
#define PN532_IRQ   (1)
#define PN532_RESET (2)

RFIDDriver rfid;

const uint8_t RFIDDriver::STD_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t RFIDDriver::U_KEY[16] = {0x43, 0x46, 0x53, 0x76, 0x31, 0x45, 0x4D, 0x55, 0x4C, 0x41, 0x54, 0x4F, 0x52, 0x31, 0x33, 0x37};
const uint8_t RFIDDriver::D_KEY[16] = {0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37, 0x13, 0x37};

RFIDDriver::RFIDDriver() : nfc(nullptr), currentUidLen(0), keyACached(false) {
    memset(currentUid, 0, sizeof(currentUid));
    memset(cachedKeyA, 0, sizeof(cachedKeyA));
}

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
    keyACached = false;
    return nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLen, 50);
}

void RFIDDriver::haltTag() {
    // No direct equivalent in Adafruit library, but this is good practice
}

// ---------------------------------------------------------------------------
// CRC-32 (IEEE 802.3 / zlib-compatible)
// Uses ESP32 ROM hardware-accelerated CRC. The ROM function uses an inverted
// initial value convention: we pass ~0 (0xFFFFFFFF) and invert the result,
// matching the standard CRC-32/ISO-HDLC algorithm (poly 0xEDB88320,
// init 0xFFFFFFFF, final XOR 0xFFFFFFFF, reflected I/O).
// ---------------------------------------------------------------------------
uint32_t RFIDDriver::computeCRC32(const uint8_t* data, size_t length) {
    return ~esp_rom_crc32_le(~0U, data, length);
}

// ---------------------------------------------------------------------------
// Sector authentication helper — caches Key A from UID
// ---------------------------------------------------------------------------
bool RFIDDriver::authenticateSector(uint8_t sector) {
    if (!nfc) return false;

    if (!keyACached) {
        generateKeyA(currentUid, cachedKeyA);
        keyACached = true;
    }

    // First block of the sector
    uint8_t firstBlock = sector * BLOCKS_PER_SECTOR;
    return nfc->mifareclassic_AuthenticateBlock(
        currentUid, currentUidLen, firstBlock, 0, cachedKeyA);
}

// ---------------------------------------------------------------------------
// Read data blocks (blocks 0-2, skipping trailer) from a range of sectors
// into a flat buffer. Returns total bytes read, or 0 on any failure.
// ---------------------------------------------------------------------------
size_t RFIDDriver::readSectorRange(uint8_t firstSector, uint8_t lastSector, uint8_t* outBuf) {
    size_t offset = 0;

    for (uint8_t sector = firstSector; sector <= lastSector; sector++) {
        if (!authenticateSector(sector)) {
            Serial.printf("Auth failed for sector %u\n", sector);
            return 0;
        }

        uint8_t firstBlock = sector * BLOCKS_PER_SECTOR;
        for (uint8_t b = 0; b < DATA_BLOCKS_PER_SECTOR; b++) {
            if (!nfc->mifareclassic_ReadDataBlock(firstBlock + b, outBuf + offset)) {
                Serial.printf("Read failed: sector %u block %u\n", sector, b);
                return 0;
            }
            offset += BYTES_PER_BLOCK;
        }
    }

    return offset;
}

// ---------------------------------------------------------------------------
// Read stored CRC32 from sector 15, block 60 (first data block of sector 15)
// ---------------------------------------------------------------------------
uint32_t RFIDDriver::readTagCRC32() {
    if (!authenticateSector(SECTOR_CONTROL)) {
        Serial.println("Auth failed for control sector 15");
        return 0;
    }

    uint8_t block[BYTES_PER_BLOCK];
    uint8_t controlBlock = SECTOR_CONTROL * BLOCKS_PER_SECTOR;  // block 60
    if (!nfc->mifareclassic_ReadDataBlock(controlBlock, block)) {
        Serial.println("Read failed for control block 60");
        return 0;
    }

    // CRC32 is stored little-endian at offset 0
    uint32_t stored = block[0] | (block[1] << 8) | (block[2] << 16) | (block[3] << 24);
    return stored;
}

// ---------------------------------------------------------------------------
// Validate tag CRC: read sector data, compute CRC, compare to stored value.
// tagVersion: TAG_VERSION_V1 (0x01) or TAG_VERSION_V2 (0x02)
// ---------------------------------------------------------------------------
bool RFIDDriver::validateTagCRC(uint8_t tagVersion, uint32_t& computedCRC, uint32_t& storedCRC) {
    uint8_t lastSector = (tagVersion >= TAG_VERSION_V2) ? CRC_V2_LAST_SECTOR : CRC_V1_LAST_SECTOR;
    size_t numSectors = lastSector - CRC_V1_FIRST_SECTOR + 1;
    size_t bufSize = numSectors * DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK;

    uint8_t* buf = (uint8_t*)malloc(bufSize);
    if (!buf) {
        Serial.println("CRC validate: malloc failed");
        return false;
    }

    size_t bytesRead = readSectorRange(CRC_V1_FIRST_SECTOR, lastSector, buf);
    if (bytesRead == 0) {
        free(buf);
        return false;
    }

    computedCRC = computeCRC32(buf, bytesRead);
    free(buf);

    storedCRC = readTagCRC32();

    bool valid = (computedCRC == storedCRC);
    Serial.printf("CRC: computed=0x%08X stored=0x%08X %s\n",
                  computedCRC, storedCRC, valid ? "OK" : "MISMATCH");
    return valid;
}

// ---------------------------------------------------------------------------
// Write CRC32 to sector 15, block 60
// ---------------------------------------------------------------------------
bool RFIDDriver::writeTagCRC32(uint32_t crc) {
    if (!authenticateSector(SECTOR_CONTROL)) {
        Serial.println("Auth failed for control sector 15 (write)");
        return false;
    }

    uint8_t block[BYTES_PER_BLOCK];
    memset(block, 0, sizeof(block));

    // Store little-endian
    block[0] = (crc >>  0) & 0xFF;
    block[1] = (crc >>  8) & 0xFF;
    block[2] = (crc >> 16) & 0xFF;
    block[3] = (crc >> 24) & 0xFF;

    uint8_t controlBlock = SECTOR_CONTROL * BLOCKS_PER_SECTOR;  // block 60
    if (!nfc->mifareclassic_WriteDataBlock(controlBlock, block)) {
        Serial.println("Write failed for control block 60");
        return false;
    }

    // Read back and verify
    uint8_t verify[BYTES_PER_BLOCK];
    if (!nfc->mifareclassic_ReadDataBlock(controlBlock, verify)) {
        Serial.println("Verify read failed for control block 60");
        return false;
    }

    if (memcmp(block, verify, BYTES_PER_BLOCK) != 0) {
        Serial.println("CRC write verification failed");
        return false;
    }

    Serial.printf("CRC32 written: 0x%08X\n", crc);
    return true;
}

// ---------------------------------------------------------------------------
// Mirror voting: read sectors 6-8, 3-way byte compare, majority select
// Per FSD Section 7.8
// ---------------------------------------------------------------------------
MirrorResult RFIDDriver::readMirrors() {
    MirrorResult result;
    memset(&result, 0, sizeof(result));
    result.badMirror = -1;

    // Read data blocks from each mirror sector (3 data blocks × 16 bytes = 48 bytes each)
    static constexpr size_t MIRROR_SIZE = DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK;  // 48 bytes
    uint8_t m0[MIRROR_SIZE], m1[MIRROR_SIZE], m2[MIRROR_SIZE];

    size_t r0 = readSectorRange(SECTOR_REMAINING_MAIN, SECTOR_REMAINING_MAIN, m0);
    size_t r1 = readSectorRange(SECTOR_REMAINING_A, SECTOR_REMAINING_A, m1);
    size_t r2 = readSectorRange(SECTOR_REMAINING_B, SECTOR_REMAINING_B, m2);

    if (r0 == 0 || r1 == 0 || r2 == 0) {
        Serial.println("Mirror read: failed to read one or more mirror sectors");
        result.valid = false;
        result.agreement = 0;
        return result;
    }

    // Compare first block of each mirror (RemainingBlock is 16 bytes in block 0)
    bool eq01 = (memcmp(m0, m1, BYTES_PER_BLOCK) == 0);
    bool eq02 = (memcmp(m0, m2, BYTES_PER_BLOCK) == 0);
    bool eq12 = (memcmp(m1, m2, BYTES_PER_BLOCK) == 0);

    const uint8_t* chosen = nullptr;

    if (eq01 && eq02) {
        // All three match — unanimous
        result.agreement = 3;
        result.badMirror = -1;
        chosen = m0;
        Serial.println("Mirrors: unanimous (3/3)");
    } else if (eq01) {
        // M0 == M1, M2 differs
        result.agreement = 2;
        result.badMirror = 2;
        chosen = m0;
        Serial.println("Mirrors: majority (2/3), sector 8 differs");
    } else if (eq02) {
        // M0 == M2, M1 differs
        result.agreement = 2;
        result.badMirror = 1;
        chosen = m0;
        Serial.println("Mirrors: majority (2/3), sector 7 differs");
    } else if (eq12) {
        // M1 == M2, M0 differs
        result.agreement = 2;
        result.badMirror = 0;
        chosen = m1;
        Serial.println("Mirrors: majority (2/3), sector 6 differs");
    } else {
        // All three differ
        result.agreement = 0;
        result.badMirror = -1;
        result.valid = false;
        Serial.println("Mirrors: ALL DIFFER — tag corrupt");
        return result;
    }

    // Copy the resolved RemainingBlock from the first data block
    memcpy(&result.data, chosen, sizeof(RemainingBlock));
    result.valid = true;

    Serial.printf("Mirror result: remain=%umm %ug, counter=%u\n",
                  result.data.remain_len_mm, result.data.remain_weight_g,
                  result.data.update_counter);
    return result;
}

// ---------------------------------------------------------------------------
// Write RemainingBlock to all 3 mirror sectors (6→7→8) with read-back verify.
// Write order per FSD 7.8: sector 6 → 7 → 8 for safe recovery on power loss.
// ---------------------------------------------------------------------------
bool RFIDDriver::writeMirrors(const RemainingBlock& data) {
    // Prepare a full block (16 bytes)
    uint8_t block[BYTES_PER_BLOCK];
    memset(block, 0, sizeof(block));
    memcpy(block, &data, sizeof(RemainingBlock));

    const uint8_t mirrorSectors[] = {
        SECTOR_REMAINING_MAIN, SECTOR_REMAINING_A, SECTOR_REMAINING_B
    };

    for (uint8_t sector : mirrorSectors) {
        if (!authenticateSector(sector)) {
            Serial.printf("Mirror write: auth failed for sector %u\n", sector);
            return false;
        }

        uint8_t firstBlock = sector * BLOCKS_PER_SECTOR;
        if (!nfc->mifareclassic_WriteDataBlock(firstBlock, block)) {
            Serial.printf("Mirror write: write failed for sector %u\n", sector);
            return false;
        }

        // Read back and verify
        uint8_t verify[BYTES_PER_BLOCK];
        if (!nfc->mifareclassic_ReadDataBlock(firstBlock, verify)) {
            Serial.printf("Mirror write: verify read failed for sector %u\n", sector);
            return false;
        }

        if (memcmp(block, verify, BYTES_PER_BLOCK) != 0) {
            Serial.printf("Mirror write: verify mismatch for sector %u\n", sector);
            return false;
        }
    }

    Serial.printf("Mirrors written: %umm %ug counter=%u\n",
                  data.remain_len_mm, data.remain_weight_g, data.update_counter);
    return true;
}

// ---------------------------------------------------------------------------
// CFS Tag Read (skeleton — reads block 4 for now)
// ---------------------------------------------------------------------------
bool RFIDDriver::readCFSTag(SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    if (!authenticateSector(SECTOR_FORMAT)) {
        Serial.println("Auth failed for sector 1");
        return false;
    }

    uint8_t block_buffer[16];
    if (!nfc->mifareclassic_ReadDataBlock(4, block_buffer)) {
        Serial.println("Read failed for block 4");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// CFS Tag Write (skeleton)
// ---------------------------------------------------------------------------
bool RFIDDriver::writeCFSTag(const SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    return true;
}

// ---------------------------------------------------------------------------
// Key A derivation from UID via AES-128
// ---------------------------------------------------------------------------
void RFIDDriver::generateKeyA(const uint8_t* uid, uint8_t* keyOut) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, U_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, uid, keyOut);
    mbedtls_aes_free(&aes);
}
