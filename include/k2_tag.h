#pragma once
#include <Arduino.h>

// Magic Constants
#define K2_MAGIC      0x4B325046  // "K2PF" — CFS tag format identifier
#define K2FX_MAGIC    0x4B324658  // "K2FX" — extended v2 origin marker

// Tag versions
#define TAG_VERSION_V1  0x01
#define TAG_VERSION_V2  0x02

// Sector Definitions — standard CFS v1
#define SECTOR_FORMAT          1
#define SECTOR_IDENTITY        2
#define SECTOR_MATERIAL        3
#define SECTOR_VENDOR          4
#define SECTOR_INIT            5
#define SECTOR_REMAINING_MAIN  6
#define SECTOR_REMAINING_A     7
#define SECTOR_REMAINING_B     8
#define SECTOR_USAGE           9

// Sector Definitions — extended v2
#define SECTOR_EXT_TEMPS      10
#define SECTOR_EXT_SPEED      11
#define SECTOR_EXT_PHYSICAL   12
#define SECTOR_EXT_NAME       13

// Control
#define SECTOR_CONTROL        15

// MIFARE Classic 1K geometry
#define BLOCKS_PER_SECTOR      4
#define BYTES_PER_BLOCK       16
#define DATA_BLOCKS_PER_SECTOR 3  // blocks 0-2 are data, block 3 is sector trailer

// CRC coverage: 3 data blocks × 16 bytes per sector
#define CRC_V1_FIRST_SECTOR    1
#define CRC_V1_LAST_SECTOR     9   // sectors 1-9 = 9 × 3 × 16 = 432 bytes
#define CRC_V2_FIRST_SECTOR    1
#define CRC_V2_LAST_SECTOR    13   // sectors 1-13 = 13 × 3 × 16 = 624 bytes

#pragma pack(push, 1)

// Sector 1: Format & Version
struct TagFormatBlock {
    uint32_t magic;       // 0x4B325046
    uint8_t version;
    uint8_t compat_mask;
    uint16_t reserved1;
    uint8_t reserved2[8];
};

// Sector 2: Filament Identity
struct FilamentIdentityBlock {
    uint32_t vendor_product_id;
    uint16_t material_type; // Enum
    uint16_t diameter_um;   // Microns
    uint8_t reserved[8];
};

// Sector 3: Material & Color
struct MaterialColorBlock {
    uint32_t color_rgb;     // 0x00RRGGBB
    char color_name[12];    // Null-padded ASCII
};

// Sector 4: Vendor Metadata
struct VendorMetaBlock {
    char vendor_name[12];
    uint32_t batch_id;      // Spec says 4 bytes, maybe char[4] or uint32? Assuming uint32 or char[4]
};

// Sector 5: Spool Init (Write Once)
struct SpoolInitBlock {
    uint32_t init_len_mm;
    uint32_t init_weight_g;
    uint8_t reserved[8];
};

// Sector 6-8: Remaining (Mutable)
struct RemainingBlock {
    uint32_t remain_len_mm;
    uint32_t remain_weight_g;
    uint32_t update_counter;
    uint32_t reserved;
};

// Sector 9: Usage
struct UsageBlock {
    uint32_t consumed_len_mm;
    uint32_t consumed_weight_g;
    uint8_t reserved[8];
};

// Sector 10: Print Temperature Settings (v2 extended)
struct ExtTempBlock {
    uint16_t nozzle_temp_min;   // °C
    uint16_t nozzle_temp_max;   // °C
    uint16_t bed_temp_min;      // °C
    uint16_t bed_temp_max;      // °C
    uint16_t nozzle_temp_default; // °C
    uint16_t bed_temp_default;  // °C
    uint32_t origin_magic;      // 0x4B324658 ("K2FX") if written by this system
};

// Sector 11: Print Speed & Fan Settings (v2 extended)
struct ExtSpeedBlock {
    uint16_t print_speed_min;   // mm/s
    uint16_t print_speed_max;   // mm/s
    uint8_t  fan_percent;       // 0-100
    uint8_t  reserved1;
    uint16_t max_volumetric_flow; // mm³/s × 10
    uint8_t  reserved2[8];
};

// Sector 12: Physical Properties (v2 extended)
struct ExtPhysicalBlock {
    uint16_t diameter_um;       // microns (e.g. 1750 = 1.75mm)
    uint16_t density_x100;      // g/cm³ × 100 (e.g. 124 = 1.24)
    char     brand[12];         // ASCII, null-padded
};

// Sector 13: Extended Name (v2 extended)
struct ExtNameBlock {
    char product_name[16];      // ASCII, null-padded
};

// Sector 15: Control
struct ControlBlock {
    uint32_t crc32;
    uint8_t reserved[12];
};

#pragma pack(pop)

class K2Tag {
public:
    TagFormatBlock format;
    FilamentIdentityBlock identity;
    MaterialColorBlock material;
    VendorMetaBlock vendor;
    SpoolInitBlock init_data;
    RemainingBlock remaining;
    UsageBlock usage;
    ControlBlock control;

    bool isValid() {
        return format.magic == K2_MAGIC;
    }
};