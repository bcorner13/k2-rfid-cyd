#include <rfid_driver.h>
#include <esp_rom_crc.h>
#include <Wire.h>

// PN532 I2C mode — connected to Mabee I2C port (HY2.0-4P, 4-pin Grove-compatible).
//
// Mabee I2C port pinout (silkscreened on PCB, pin 1 = bottom/GND):
//   Pin 1: GND
//   Pin 2: +3V3
//   Pin 3: SDA  → GPIO17  (Wire, shared with GT911 touch + PCF8563 RTC)
//   Pin 4: SCL  → GPIO18
//
// PN532 DIP switch: S1=ON, S2=OFF (I2C mode). I2C address: 0x24.
// No address conflict: GT911=0x5D, PCF8563=0x51, PN532=0x24.
//
// IRQ and RESET are not wired through the 4-pin Mabee connector.
// 0xFF passed to constructor → library falls back to polling (delay-based ready check).
// Hardware reset happens at power-on; software reset not needed for normal operation.
#define PN532_IRQ    (0xFF) // Not connected — polling mode (0xFF = no-pin sentinel)
#define PN532_RESET  (0xFF) // Not connected — power-on reset only (0xFF = no-pin sentinel)

RFIDDriver rfid;

// Last CFS tag dump — populated by readCFSTag(), shown as hex log on RFID Raw screen.
static char s_rawDump[1024] = {};

// Last decoded CFS payload string — 40-char uppercase ASCII (e.g. "5C3250276A210...").
// Populated only on a successful readCFSTag(); empty if no tag has been read yet.
static char s_lastPayload[48] = {};

// Last mirror read result — populated inside readCFSTag() immediately after payload read,
// while the tag is still selected in the same MIFARE session.
static MirrorResult s_lastMirrors = {};

const MirrorResult& RFIDDriver::getLastMirrors() const { return s_lastMirrors; }

const char* RFIDDriver::getLastRawDump() const { return s_rawDump; }
const char* RFIDDriver::getLastPayload()  const { return s_lastPayload; }

const uint8_t RFIDDriver::STD_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// AES-128 master key for Key A derivation — from DnG-Crafts/K2-RFID (Utils.cs CreateKey)
const uint8_t RFIDDriver::U_KEY[16] = {0x71, 0x33, 0x62, 0x75, 0x5E, 0x74, 0x31, 0x6E,
                                        0x71, 0x66, 0x5A, 0x28, 0x70, 0x66, 0x24, 0x31};
// AES-128 key for payload block encryption — from DnG-Crafts/K2-RFID (CipherData)
const uint8_t RFIDDriver::D_KEY[16] = {0x48, 0x40, 0x43, 0x46, 0x6B, 0x52, 0x6E, 0x7A,
                                        0x40, 0x4B, 0x41, 0x74, 0x42, 0x4A, 0x70, 0x32};

RFIDDriver::RFIDDriver() : nfc(nullptr), currentUidLen(0), keyACached(false) {
    memset(currentUid, 0, sizeof(currentUid));
    memset(cachedKeyA, 0, sizeof(cachedKeyA));
}

void RFIDDriver::init() {
    // Wire.begin(17, 18) is called in main.cpp setup() before rfid.init().
    nfc = new Adafruit_PN532(PN532_IRQ, PN532_RESET, &Wire);
    nfc->begin();

    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
        Serial.println("RFID: PN532 not found on I2C (0x24) — check cable and DIP switches");
    } else {
        Serial.printf("RFID: PN532 found — IC=0x%02X ver=%u.%u\n",
                      (versiondata >> 24) & 0xFF,
                      (versiondata >> 16) & 0xFF,
                      (versiondata >>  8) & 0xFF);
        nfc->SAMConfig();
    }
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
//
// IMPORTANT: MIFARE Classic protocol behaviour after a failed authentication:
//   The card transitions to a deselected/halt state. Any subsequent I2C command
//   to that card (including a second auth attempt) will fail unless the card is
//   re-selected first via a fresh InListPassiveTarget / readPassiveTargetID call.
//
// Strategy: try derived Key A first; if it fails, re-detect the card (which
// re-selects it), then try standard key (0xFF×6) as a fallback for factory tags.
// ---------------------------------------------------------------------------
bool RFIDDriver::authenticateSector(uint8_t sector) {
    if (!nfc) return false;

    if (!keyACached) {
        generateKeyA(currentUid, cachedKeyA);
        keyACached = true;
        // Log UID + derived key together so they can be cross-checked against
        // reference tools (e.g. DnG-Crafts/K2-RFID for the same UID).
        Serial.printf("Auth: UID");
        for (uint8_t i = 0; i < currentUidLen; i++) Serial.printf(" %02X", currentUid[i]);
        Serial.printf("  key: %02X %02X %02X %02X %02X %02X\n",
                      cachedKeyA[0], cachedKeyA[1], cachedKeyA[2],
                      cachedKeyA[3], cachedKeyA[4], cachedKeyA[5]);
    }

    uint8_t firstBlock = sector * BLOCKS_PER_SECTOR;

    // Attempt 1: derived Key A
    if (nfc->mifareclassic_AuthenticateBlock(currentUid, currentUidLen, firstBlock, 0, cachedKeyA)) {
        return true;
    }
    Serial.printf("Auth: derived key failed sector %u — re-selecting tag\n", sector);

    // After a failed MIFARE auth the card is deselected; re-detect it before retrying.
    if (!nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLen, 200)) {
        Serial.println("Auth: tag lost after re-select attempt");
        return false;
    }

    // Attempt 2: standard key fallback (factory / uninitialized tags)
    if (nfc->mifareclassic_AuthenticateBlock(currentUid, currentUidLen, firstBlock, 0, (uint8_t*)STD_KEY)) {
        Serial.printf("Auth: standard key succeeded sector %u\n", sector);
        return true;
    }

    Serial.printf("Auth: both keys failed sector %u\n", sector);
    return false;
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
// P0.6: Format UID as colon-separated hex string
// ---------------------------------------------------------------------------
String RFIDDriver::formatUID() const {
    if (currentUidLen == 0) return "";

    String result;
    result.reserve(currentUidLen * 3);  // "XX:XX:XX:XX" + null
    char hex[4];
    for (uint8_t i = 0; i < currentUidLen; i++) {
        if (i > 0) result += ':';
        snprintf(hex, sizeof(hex), "%02X", currentUid[i]);
        result += hex;
    }
    return result;
}

// ---------------------------------------------------------------------------
// P0.6: Weight reconciliation — compare tag vs inventory with ±5g tolerance
// ---------------------------------------------------------------------------
WeightReconcileResult RFIDDriver::reconcileWeight(uint32_t tag_weight_g, uint32_t inventory_weight_g) {
    WeightReconcileResult r;
    r.tag_weight_g = tag_weight_g;
    r.inventory_weight_g = inventory_weight_g;
    r.delta_g = static_cast<int32_t>(tag_weight_g) - static_cast<int32_t>(inventory_weight_g);

    int32_t abs_delta = (r.delta_g < 0) ? -r.delta_g : r.delta_g;
    r.in_sync = (static_cast<uint32_t>(abs_delta) <= WEIGHT_TOLERANCE_G);

    Serial.printf("Reconcile: tag=%ug inv=%ug delta=%dg %s\n",
                  tag_weight_g, inventory_weight_g, r.delta_g,
                  r.in_sync ? "IN_SYNC" : "MISMATCH");
    return r;
}

// ---------------------------------------------------------------------------
// P0.7: Read tag version byte from sector 1 (format block)
// ---------------------------------------------------------------------------
uint8_t RFIDDriver::readTagVersion() {
    if (!authenticateSector(SECTOR_FORMAT)) {
        Serial.println("readTagVersion: auth failed for sector 1");
        return 0;
    }

    uint8_t block[BYTES_PER_BLOCK];
    uint8_t firstBlock = SECTOR_FORMAT * BLOCKS_PER_SECTOR;  // block 4
    if (!nfc->mifareclassic_ReadDataBlock(firstBlock, block)) {
        Serial.println("readTagVersion: read failed for block 4");
        return 0;
    }

    // Interpret as TagFormatBlock — version is at offset 4
    TagFormatBlock* fmt = reinterpret_cast<TagFormatBlock*>(block);
    if (fmt->magic != K2_MAGIC) {
        Serial.printf("readTagVersion: bad magic 0x%08X (expected 0x%08X)\n", fmt->magic, K2_MAGIC);
        return 0;
    }

    Serial.printf("Tag version: %u\n", fmt->version);
    return fmt->version;
}

// ---------------------------------------------------------------------------
// P0.7: Full version info including v2 origin detection
// ---------------------------------------------------------------------------
TagVersionInfo RFIDDriver::readTagVersionInfo() {
    TagVersionInfo info;
    memset(&info, 0, sizeof(info));

    info.version = readTagVersion();
    info.is_v2 = (info.version >= TAG_VERSION_V2);

    if (!info.is_v2) {
        // v1 tag — no extended sectors to check
        return info;
    }

    // Read sector 10, block 0 to get origin_magic from ExtTempBlock
    if (!authenticateSector(SECTOR_EXT_TEMPS)) {
        Serial.println("readTagVersionInfo: auth failed for sector 10");
        // Still report v2, just can't confirm origin
        return info;
    }

    uint8_t block[BYTES_PER_BLOCK];
    uint8_t firstBlock = SECTOR_EXT_TEMPS * BLOCKS_PER_SECTOR;
    if (!nfc->mifareclassic_ReadDataBlock(firstBlock, block)) {
        Serial.println("readTagVersionInfo: read failed for sector 10 block 0");
        return info;
    }

    // ExtTempBlock: origin_magic is at offset 12 (after 6 × uint16_t)
    ExtTempBlock* temp = reinterpret_cast<ExtTempBlock*>(block);
    info.origin_magic = temp->origin_magic;
    info.is_our_v2 = (info.origin_magic == K2FX_MAGIC);

    Serial.printf("V2 origin: magic=0x%08X %s\n",
                  info.origin_magic,
                  info.is_our_v2 ? "(ours)" : "(FOREIGN)");
    return info;
}

// ---------------------------------------------------------------------------
// P1.1: Full tag read into TagData (FSD Section 7.9)
// Reads all sectors, performs mirror voting, validates CRC, populates fields.
// ---------------------------------------------------------------------------
bool RFIDDriver::readTag(TagData& out) {
    memset(&out, 0, sizeof(TagData));

    if (!nfc || !checkTagPresent()) return false;

    // Copy UID
    memcpy(out.uid, currentUid, currentUidLen);
    out.uid_length = currentUidLen;

    // Read format block (sector 1) for version
    if (!authenticateSector(SECTOR_FORMAT)) {
        Serial.println("readTag: auth failed sector 1");
        return false;
    }

    uint8_t block[BYTES_PER_BLOCK];
    uint8_t firstBlock = SECTOR_FORMAT * BLOCKS_PER_SECTOR;
    if (!nfc->mifareclassic_ReadDataBlock(firstBlock, block)) {
        Serial.println("readTag: read failed sector 1 block 0");
        return false;
    }

    TagFormatBlock* fmt = reinterpret_cast<TagFormatBlock*>(block);
    if (fmt->magic != K2_MAGIC) {
        Serial.printf("readTag: bad magic 0x%08X\n", fmt->magic);
        return false;
    }
    out.version = fmt->version;

    // Read payload sectors 1-4 (all data blocks)
    uint8_t payload_buf[4 * DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK];  // 192 bytes
    size_t payload_bytes = readSectorRange(SECTOR_FORMAT, SECTOR_VENDOR, payload_buf);
    if (payload_bytes == 0) {
        Serial.println("readTag: failed to read payload sectors 1-4");
        return false;
    }

    // Extract CFS payload string from first ~34 bytes (spans sector 1 data)
    // The payload string starts at the identity block in sector 2
    // For now, read sector 2 block 0 for identity, sector 3 for material/color
    // The actual CFS payload string format is spread across sectors

    // Read init weight (sector 5)
    if (authenticateSector(SECTOR_INIT)) {
        uint8_t init_block[BYTES_PER_BLOCK];
        if (nfc->mifareclassic_ReadDataBlock(SECTOR_INIT * BLOCKS_PER_SECTOR, init_block)) {
            SpoolInitBlock* init = reinterpret_cast<SpoolInitBlock*>(init_block);
            out.init_length_mm = init->init_len_mm;
            out.init_weight_g = init->init_weight_g;
        }
    }

    // Mirror voting for remaining data (sectors 6-8)
    MirrorResult mirrors = readMirrors();
    out.mirror_agreement = mirrors.agreement;
    if (mirrors.valid) {
        out.remaining_length_mm = mirrors.data.remain_len_mm;
        out.remaining_weight_g = mirrors.data.remain_weight_g;
        out.update_counter = mirrors.data.update_counter;
    }

    // Read usage (sector 9)
    if (authenticateSector(SECTOR_USAGE)) {
        uint8_t usage_block[BYTES_PER_BLOCK];
        if (nfc->mifareclassic_ReadDataBlock(SECTOR_USAGE * BLOCKS_PER_SECTOR, usage_block)) {
            UsageBlock* usage = reinterpret_cast<UsageBlock*>(usage_block);
            out.usage_counter = usage->consumed_len_mm;  // Using consumed_len as counter proxy
        }
    }

    // CRC validation
    out.crc_valid = validateTagCRC(out.version, out.crc32_computed, out.crc32_stored);

    // v2 extended fields (sectors 10-13)
    if (out.version >= TAG_VERSION_V2) {
        // Sector 10: temps + origin magic
        if (authenticateSector(SECTOR_EXT_TEMPS)) {
            uint8_t temp_block[BYTES_PER_BLOCK];
            if (nfc->mifareclassic_ReadDataBlock(SECTOR_EXT_TEMPS * BLOCKS_PER_SECTOR, temp_block)) {
                ExtTempBlock* temps = reinterpret_cast<ExtTempBlock*>(temp_block);
                out.nozzle_temp_min = temps->nozzle_temp_min;
                out.nozzle_temp_max = temps->nozzle_temp_max;
                out.bed_temp_min = temps->bed_temp_min;
                out.bed_temp_max = temps->bed_temp_max;
                out.nozzle_temp_default = temps->nozzle_temp_default;
                out.bed_temp_default = temps->bed_temp_default;
                out.origin_magic = temps->origin_magic;
                out.has_extended = (out.origin_magic == K2FX_MAGIC);
            }
        }

        // Sector 11: speed + fan
        if (authenticateSector(SECTOR_EXT_SPEED)) {
            uint8_t speed_block[BYTES_PER_BLOCK];
            if (nfc->mifareclassic_ReadDataBlock(SECTOR_EXT_SPEED * BLOCKS_PER_SECTOR, speed_block)) {
                ExtSpeedBlock* speed = reinterpret_cast<ExtSpeedBlock*>(speed_block);
                out.print_speed_min = speed->print_speed_min;
                out.print_speed_max = speed->print_speed_max;
                out.fan_percent = speed->fan_percent;
                out.max_volumetric_flow = speed->max_volumetric_flow;
            }
        }

        // Sector 12: physical properties
        if (authenticateSector(SECTOR_EXT_PHYSICAL)) {
            uint8_t phys_block[BYTES_PER_BLOCK];
            if (nfc->mifareclassic_ReadDataBlock(SECTOR_EXT_PHYSICAL * BLOCKS_PER_SECTOR, phys_block)) {
                ExtPhysicalBlock* phys = reinterpret_cast<ExtPhysicalBlock*>(phys_block);
                out.diameter_um = phys->diameter_um;
                out.density_x100 = phys->density_x100;
                memcpy(out.brand, phys->brand, 12);
                out.brand[12] = '\0';
            }
        }

        // Sector 13: product name
        if (authenticateSector(SECTOR_EXT_NAME)) {
            uint8_t name_block[BYTES_PER_BLOCK];
            if (nfc->mifareclassic_ReadDataBlock(SECTOR_EXT_NAME * BLOCKS_PER_SECTOR, name_block)) {
                ExtNameBlock* name_data = reinterpret_cast<ExtNameBlock*>(name_block);
                memcpy(out.product_name, name_data->product_name, 16);
                out.product_name[16] = '\0';
            }
        }
    }

    Serial.printf("readTag: v%u uid=%s crc=%s mirrors=%u/3\n",
                  out.version, out.formatUID().c_str(),
                  out.crc_valid ? "OK" : "FAIL", out.mirror_agreement);
    return true;
}

// ---------------------------------------------------------------------------
// P1.1: Full tag write from TagData (FSD Section 7.9)
// Writes payload to sectors 1-9, extended to 10-13 (if v2), CRC to 15.
// ---------------------------------------------------------------------------
bool RFIDDriver::writeTag(const TagData& in) {
    if (!nfc || !checkTagPresent()) return false;

    // Sector 1: Format block
    {
        TagFormatBlock fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.magic = K2_MAGIC;
        fmt.version = in.version;
        fmt.compat_mask = (in.version >= TAG_VERSION_V2) ? 0x03 : 0x01;

        uint8_t block[BYTES_PER_BLOCK];
        memset(block, 0, BYTES_PER_BLOCK);
        memcpy(block, &fmt, sizeof(fmt));

        if (!authenticateSector(SECTOR_FORMAT)) return false;
        if (!nfc->mifareclassic_WriteDataBlock(SECTOR_FORMAT * BLOCKS_PER_SECTOR, block)) {
            Serial.println("writeTag: failed sector 1");
            return false;
        }
    }

    // Sector 5: Init weight
    {
        SpoolInitBlock init;
        memset(&init, 0, sizeof(init));
        init.init_len_mm = in.init_length_mm;
        init.init_weight_g = in.init_weight_g;

        uint8_t block[BYTES_PER_BLOCK];
        memset(block, 0, BYTES_PER_BLOCK);
        memcpy(block, &init, sizeof(init));

        if (!authenticateSector(SECTOR_INIT)) return false;
        if (!nfc->mifareclassic_WriteDataBlock(SECTOR_INIT * BLOCKS_PER_SECTOR, block)) {
            Serial.println("writeTag: failed sector 5");
            return false;
        }
    }

    // Sectors 6-8: Mirrors (remaining weight)
    {
        RemainingBlock remain;
        memset(&remain, 0, sizeof(remain));
        remain.remain_len_mm = in.remaining_length_mm;
        remain.remain_weight_g = in.remaining_weight_g;
        remain.update_counter = in.update_counter;

        if (!writeMirrors(remain)) {
            Serial.println("writeTag: failed mirrors");
            return false;
        }
    }

    // v2 extended sectors 10-13
    if (in.version >= TAG_VERSION_V2 && in.has_extended) {
        // Sector 10: Temps + origin magic
        {
            ExtTempBlock temps;
            memset(&temps, 0, sizeof(temps));
            temps.nozzle_temp_min = in.nozzle_temp_min;
            temps.nozzle_temp_max = in.nozzle_temp_max;
            temps.bed_temp_min = in.bed_temp_min;
            temps.bed_temp_max = in.bed_temp_max;
            temps.nozzle_temp_default = in.nozzle_temp_default;
            temps.bed_temp_default = in.bed_temp_default;
            temps.origin_magic = in.origin_magic;

            uint8_t block[BYTES_PER_BLOCK];
            memset(block, 0, BYTES_PER_BLOCK);
            memcpy(block, &temps, sizeof(temps));

            if (!authenticateSector(SECTOR_EXT_TEMPS)) return false;
            if (!nfc->mifareclassic_WriteDataBlock(SECTOR_EXT_TEMPS * BLOCKS_PER_SECTOR, block)) {
                Serial.println("writeTag: failed sector 10");
                return false;
            }
        }

        // Sector 11: Speed + fan
        {
            ExtSpeedBlock speed;
            memset(&speed, 0, sizeof(speed));
            speed.print_speed_min = in.print_speed_min;
            speed.print_speed_max = in.print_speed_max;
            speed.fan_percent = in.fan_percent;
            speed.max_volumetric_flow = in.max_volumetric_flow;

            uint8_t block[BYTES_PER_BLOCK];
            memset(block, 0, BYTES_PER_BLOCK);
            memcpy(block, &speed, sizeof(speed));

            if (!authenticateSector(SECTOR_EXT_SPEED)) return false;
            if (!nfc->mifareclassic_WriteDataBlock(SECTOR_EXT_SPEED * BLOCKS_PER_SECTOR, block)) {
                Serial.println("writeTag: failed sector 11");
                return false;
            }
        }

        // Sector 12: Physical properties + brand
        {
            ExtPhysicalBlock phys;
            memset(&phys, 0, sizeof(phys));
            phys.diameter_um = in.diameter_um;
            phys.density_x100 = in.density_x100;
            memcpy(phys.brand, in.brand, 12);

            uint8_t block[BYTES_PER_BLOCK];
            memset(block, 0, BYTES_PER_BLOCK);
            memcpy(block, &phys, sizeof(phys));

            if (!authenticateSector(SECTOR_EXT_PHYSICAL)) return false;
            if (!nfc->mifareclassic_WriteDataBlock(SECTOR_EXT_PHYSICAL * BLOCKS_PER_SECTOR, block)) {
                Serial.println("writeTag: failed sector 12");
                return false;
            }
        }

        // Sector 13: Product name
        {
            ExtNameBlock name_blk;
            memset(&name_blk, 0, sizeof(name_blk));
            memcpy(name_blk.product_name, in.product_name, 16);

            uint8_t block[BYTES_PER_BLOCK];
            memset(block, 0, BYTES_PER_BLOCK);
            memcpy(block, &name_blk, sizeof(name_blk));

            if (!authenticateSector(SECTOR_EXT_NAME)) return false;
            if (!nfc->mifareclassic_WriteDataBlock(SECTOR_EXT_NAME * BLOCKS_PER_SECTOR, block)) {
                Serial.println("writeTag: failed sector 13");
                return false;
            }
        }
    }

    // Compute and write CRC
    uint8_t lastSector = (in.version >= TAG_VERSION_V2) ? CRC_V2_LAST_SECTOR : CRC_V1_LAST_SECTOR;
    size_t numSectors = lastSector - CRC_V1_FIRST_SECTOR + 1;
    size_t bufSize = numSectors * DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK;

    uint8_t* buf = (uint8_t*)malloc(bufSize);
    if (!buf) {
        Serial.println("writeTag: CRC malloc failed");
        return false;
    }

    size_t bytesRead = readSectorRange(CRC_V1_FIRST_SECTOR, lastSector, buf);
    if (bytesRead == 0) {
        free(buf);
        Serial.println("writeTag: CRC read-back failed");
        return false;
    }

    uint32_t crc = computeCRC32(buf, bytesRead);
    free(buf);

    if (!writeTagCRC32(crc)) {
        Serial.println("writeTag: CRC write failed");
        return false;
    }

    Serial.printf("writeTag: v%u CRC=0x%08X OK\n", in.version, crc);
    return true;
}

// ---------------------------------------------------------------------------
// AES-128-ECB block decrypt/encrypt using D_KEY (for CFS payload blocks).
// ---------------------------------------------------------------------------
bool RFIDDriver::decryptBlock(const uint8_t* input, uint8_t* output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, D_KEY, 128);
    int ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

bool RFIDDriver::encryptBlock(const uint8_t* input, uint8_t* output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, D_KEY, 128);
    int ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0);
}

// ---------------------------------------------------------------------------
// CFS Tag Read — reads sector 1 (blocks 4-6), tries raw then D_KEY-decrypted
// to find the 34-char ASCII CFS payload string, then parses into SpoolData.
// Hex dump (raw + decrypted) printed to Serial for diagnostics.
// ---------------------------------------------------------------------------
bool RFIDDriver::readCFSTag(SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    // Read sector 1 data blocks (blocks 4, 5, 6 = 48 bytes).
    // The CFS payload string starts at block 4 as ASCII (possibly encrypted
    // with D_KEY on Creality v1 tags).
    uint8_t rawBuf[DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK];
    if (readSectorRange(SECTOR_FORMAT, SECTOR_FORMAT, rawBuf) == 0) return false;

    // Decrypt each 16-byte block with D_KEY.
    uint8_t decBuf[DATA_BLOCKS_PER_SECTOR * BYTES_PER_BLOCK];
    for (int b = 0; b < (int)DATA_BLOCKS_PER_SECTOR; b++) {
        const uint8_t* in  = rawBuf + b * BYTES_PER_BLOCK;
        uint8_t*       out = decBuf + b * BYTES_PER_BLOCK;
        if (!decryptBlock(in, out)) memcpy(out, in, BYTES_PER_BLOCK);
    }

    // Build rich hex dump into s_rawDump (for RFID Raw screen) and Serial.
    char* p = s_rawDump;
    char* end = s_rawDump + sizeof(s_rawDump) - 1;
    auto dump_append = [&](const char* line) {
        size_t rem = end - p;
        if (rem > 1) { strncpy(p, line, rem); p += strnlen(line, rem); }
    };

    // UID line
    char line[128];
    snprintf(line, sizeof(line), "UID:");
    for (int i = 0; i < (int)currentUidLen; i++) snprintf(line + strlen(line), sizeof(line) - strlen(line), " %02X", currentUid[i]);
    strncat(line, "\n", sizeof(line) - strlen(line) - 1);
    dump_append(line);

    // Per-block hex dump
    Serial.println("CFS sector 1 (blocks 4-6):");
    for (int b = 0; b < (int)DATA_BLOCKS_PER_SECTOR; b++) {
        const uint8_t* r = rawBuf + b * BYTES_PER_BLOCK;
        const uint8_t* d = decBuf + b * BYTES_PER_BLOCK;

        // Serial output
        Serial.printf("  B%d raw:", 4 + b);
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) Serial.printf(" %02X", r[i]);
        Serial.printf("  |");
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) Serial.printf("%c", (r[i] >= 0x20 && r[i] < 0x7F) ? r[i] : '.');
        Serial.println("|");
        Serial.printf("  B%d dec:", 4 + b);
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) Serial.printf(" %02X", d[i]);
        Serial.printf("  |");
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) Serial.printf("%c", (d[i] >= 0x20 && d[i] < 0x7F) ? d[i] : '.');
        Serial.println("|");

        // Screen dump — raw row
        snprintf(line, sizeof(line), "B%d raw:", 4 + b);
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) snprintf(line + strlen(line), sizeof(line) - strlen(line), " %02X", r[i]);
        strncat(line, "\n", sizeof(line) - strlen(line) - 1);
        dump_append(line);

        // Screen dump — dec row + ASCII
        snprintf(line, sizeof(line), "B%d dec:", 4 + b);
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) snprintf(line + strlen(line), sizeof(line) - strlen(line), " %02X", d[i]);
        strncat(line, "\n     |", sizeof(line) - strlen(line) - 1);
        for (int i = 0; i < (int)BYTES_PER_BLOCK; i++) snprintf(line + strlen(line), sizeof(line) - strlen(line), "%c", (d[i] >= 0x20 && d[i] < 0x7F) ? d[i] : '.');
        strncat(line, "|\n", sizeof(line) - strlen(line) - 1);
        dump_append(line);
    }
    *p = '\0';

    // Validity check: a real CFS payload is 34 printable-ASCII chars.
    auto isPlausibleCFS = [](const uint8_t* buf) -> bool {
        for (int i = 0; i < 34; i++) {
            if (buf[i] < 0x20 || buf[i] > 0x7E) return false;
        }
        return true;
    };

    // Try raw first, then decrypted (handles both unencrypted and encrypted tags).
    const uint8_t* bufs[2] = { rawBuf, decBuf };
    const char*    names[2] = { "raw", "dec" };
    for (int attempt = 0; attempt < 2; attempt++) {
        const uint8_t* src = bufs[attempt];
        if (!isPlausibleCFS(src)) continue;

        char tmp[49];
        memcpy(tmp, src, 48);
        tmp[48] = '\0';
        std::string payload(tmp);
        SpoolData parsed(payload);
        if (parsed.getRawData().empty()) continue;

        spoolData = parsed;

        // Cache the decoded payload string for the RFID Raw table screen.
        const std::string& raw = parsed.getRawData();
        strncpy(s_lastPayload, raw.c_str(), sizeof(s_lastPayload) - 1);
        s_lastPayload[sizeof(s_lastPayload) - 1] = '\0';

        // Append parse result to existing hex dump in s_rawDump
        size_t used = strlen(s_rawDump);
        snprintf(s_rawDump + used, sizeof(s_rawDump) - used,
            "\nPAYLOAD(%s):\n%s\ntype=%-5s  wt=%ug  %s",
            names[attempt],
            parsed.getRawData().c_str(),
            parsed.getType().c_str(),
            parsed.getWeight(),
            parsed.getColorName().c_str());
        Serial.printf("CFS OK (%s): [%s] type=%s wt=%ug\n",
                      names[attempt],
                      parsed.getRawData().c_str(),
                      parsed.getType().c_str(),
                      parsed.getWeight());

        // Read mirror sectors (6-8) now, while tag is still selected in this session.
        // Storing result so callers can retrieve remaining filament without a second session.
        s_lastMirrors = readMirrors();
        if (s_lastMirrors.valid) {
            Serial.printf("Mirrors: remain=%umm / %ug  counter=%u\n",
                          s_lastMirrors.data.remain_len_mm,
                          s_lastMirrors.data.remain_weight_g,
                          s_lastMirrors.data.update_counter);
        } else {
            Serial.println("Mirrors: unreadable (mutable sectors may use different keys)");
        }

        return true;
    }

    // Neither buffer had a valid CFS payload — append failure note to dump.
    size_t used = strlen(s_rawDump);
    snprintf(s_rawDump + used, sizeof(s_rawDump) - used, "\nNo CFS payload found");
    Serial.println("CFS: no valid CFS payload in raw or decrypted data");
    return false;
}

// ---------------------------------------------------------------------------
// CFS Tag Write (skeleton)
// ---------------------------------------------------------------------------
bool RFIDDriver::writeCFSTag(const SpoolData& spoolData) {
    if (!nfc || !checkTagPresent()) return false;

    return true;
}

// ---------------------------------------------------------------------------
// Key A derivation from UID via AES-128-ECB (CFS spec).
//
// Two critical correctness requirements:
//  1. AES input must be exactly 16 bytes: zero-pad the UID (4 bytes for MIFARE
//     Classic) into a 16-byte block rather than passing the raw UID pointer,
//     which would cause a 12-byte read overrun into adjacent struct members.
//  2. AES output is 16 bytes but Key A is only 6: use a 16-byte stack buffer
//     for the AES result and copy the first 6 bytes to keyOut.  Writing 16
//     bytes directly to a 6-byte keyOut buffer overwrites keyACached plus
//     9 bytes past the end of the RFIDDriver object, corrupting whatever
//     global follows it in BSS (sysState.currentState in practice).
// ---------------------------------------------------------------------------
void RFIDDriver::generateKeyA(const uint8_t* uid, uint8_t* keyOut) {
    // Cycle the 4-byte UID to fill a 16-byte AES input block: uid[i % 4].
    // Reference: DnG-Crafts/K2-RFID Utils.cs CreateKey() — the UID is NOT zero-padded.
    uint8_t input[16];
    for (int i = 0; i < 16; i++) input[i] = uid[i % currentUidLen];

    uint8_t aes_out[16];
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, U_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, aes_out);
    mbedtls_aes_free(&aes);

    memcpy(keyOut, aes_out, 6);  // Key A = first 6 bytes of AES output
}
