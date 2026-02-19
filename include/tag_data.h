#pragma once
/**
 * @file tag_data.h
 * @brief Typed abstraction over raw RFID tag bytes (FSD Section 7.9).
 *
 * TagData encapsulates v1/v2 format differences so no other module needs
 * to understand sector layouts, byte offsets, or CRC mechanics.
 * See FSD Section 3.2 for how it relates to FilamentProfile and SpoolRecord.
 */

#include <Arduino.h>
#include "k2_tag.h"

struct SpoolRecord;  // Forward declaration

struct TagData {
    // Identity
    uint8_t  uid[7];                // 7-byte MIFARE UID
    uint8_t  uid_length;            // 4 or 7
    uint8_t  version;               // TAG_VERSION_V1 or TAG_VERSION_V2

    // v1 payload (parsed from sectors 1-4)
    char     payload_string[41];    // Fixed-length CFS payload (null-terminated)
    char     material_type[6];      // 5 chars + null (from payload pos 12-16)
    uint32_t color_hex;             // Parsed from payload pos 18-23
    uint16_t material_length_mm;    // Parsed from payload pos 24-27

    // Initial weight (sector 5)
    uint32_t init_weight_g;
    uint32_t init_length_mm;

    // Mutable data (sectors 6-8 mirrors, sector 9)
    uint32_t remaining_length_mm;   // From mirror voting (FSD 7.8)
    uint32_t remaining_weight_g;    // From mirror voting
    uint32_t update_counter;        // Mirror update counter
    uint32_t usage_counter;         // Sector 9 usage count

    // v2 extended fields (sectors 10-13, only populated if version >= TAG_VERSION_V2)
    bool     has_extended;          // true if v2 with K2FX origin magic
    uint16_t nozzle_temp_min;
    uint16_t nozzle_temp_max;
    uint16_t bed_temp_min;
    uint16_t bed_temp_max;
    uint16_t nozzle_temp_default;
    uint16_t bed_temp_default;
    uint16_t print_speed_min;
    uint16_t print_speed_max;
    uint8_t  fan_percent;
    uint16_t max_volumetric_flow;   // x10 (e.g., 240 = 24.0 mm3/s)
    uint16_t diameter_um;
    uint16_t density_x100;          // x100 (e.g., 127 = 1.27 g/cm3)
    char     brand[13];             // 12 chars + null (sector 12)
    char     product_name[17];      // 16 chars + null (sector 13)
    uint32_t origin_magic;          // Sector 10 offset 0x0C (K2FX_MAGIC if ours)

    // Integrity
    uint32_t crc32_stored;          // CRC from sector 15
    uint32_t crc32_computed;        // CRC computed over read data
    bool     crc_valid;             // crc32_stored == crc32_computed
    uint8_t  mirror_agreement;      // 3 = unanimous, 2 = majority, 0 = all differ

    // Format UID as colon-separated hex string
    String formatUID() const;

    // Weight/length conversion helpers (same formula as SpoolData)
    static uint32_t lengthToWeight(uint32_t length_mm);
    static uint32_t weightToLength(uint32_t weight_g);
};

// Conversion functions (FSD Section 3.2)
// SpoolRecord -> TagData: serialize spool fields into tag format
void tagDataFromSpool(const SpoolRecord& spool, TagData& out);

// TagData -> SpoolRecord: parse tag fields into inventory record
void spoolFromTagData(const TagData& tag, SpoolRecord& out);
