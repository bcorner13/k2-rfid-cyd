// This file is not being referenced anywhere
#pragma once
#include <Arduino.h>

// Magic Constant: "K2PF" -> 0x4B325046
#define K2_MAGIC 0x4B325046

// Sector Definitions
#define SECTOR_FORMAT 1
#define SECTOR_IDENTITY 2
#define SECTOR_MATERIAL 3
#define SECTOR_VENDOR 4
#define SECTOR_INIT 5
#define SECTOR_REMAINING_MAIN 6
#define SECTOR_REMAINING_A 7
#define SECTOR_REMAINING_B 8
#define SECTOR_USAGE 9
#define SECTOR_CONTROL 15

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