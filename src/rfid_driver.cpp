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
