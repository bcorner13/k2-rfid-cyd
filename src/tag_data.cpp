#include <tag_data.h>
#include <inventory_manager.h>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Weight/length conversion: len = 330 * weight_g / 1000
// Same formula as SpoolData (docs/rfid/creality-k2plus-rfid-spec.md)
// ---------------------------------------------------------------------------
uint32_t TagData::lengthToWeight(uint32_t length_mm) {
    return 1000 * length_mm / 330;
}

uint32_t TagData::weightToLength(uint32_t weight_g) {
    return 330 * weight_g / 1000;
}

// ---------------------------------------------------------------------------
// Format UID as "04:A3:2B:1C" style string
// ---------------------------------------------------------------------------
String TagData::formatUID() const {
    if (uid_length == 0) return "";

    String result;
    result.reserve(uid_length * 3);
    char hex[4];
    for (uint8_t i = 0; i < uid_length; i++) {
        if (i > 0) result += ':';
        snprintf(hex, sizeof(hex), "%02X", uid[i]);
        result += hex;
    }
    return result;
}

// ---------------------------------------------------------------------------
// SpoolRecord -> TagData: serialize for tag write
// ---------------------------------------------------------------------------
void tagDataFromSpool(const SpoolRecord& spool, TagData& out) {
    memset(&out, 0, sizeof(TagData));

    // Version: always write v2 if we have extended fields
    out.version = TAG_VERSION_V2;
    out.has_extended = true;
    out.origin_magic = K2FX_MAGIC;

    // Build the v1 payload string (34 chars)
    // Format: DATE(5) + VENDOR(4) + BATCH(2) + "1" + TYPE(5) + "0" + COLOR(6) + LEN(4) + SERIAL(6)
    char payload[41];
    memset(payload, 0, sizeof(payload));

    // Default header fields
    const char* date = "AB124";
    const char* vendor = "0276";
    const char* batch = "A2";

    // Material type, space-padded to 5 chars
    char mat_type[6];
    memset(mat_type, ' ', 5);
    mat_type[5] = '\0';
    size_t mt_len = spool.material_type.length();
    if (mt_len > 5) mt_len = 5;
    memcpy(mat_type, spool.material_type.c_str(), mt_len);

    // Color hex from string
    uint32_t color = strtoul(spool.color_hex.c_str(), nullptr, 16);

    // Weight to length
    uint32_t len_mm = TagData::weightToLength(spool.current_weight_g);
    if (len_mm > 9999) len_mm = 9999;

    snprintf(payload, sizeof(payload), "%s%s%s1%s0%06X%04u%06u",
             date, vendor, batch, mat_type, color & 0xFFFFFF,
             (unsigned)len_mm, (unsigned)(millis() % 1000000));

    // Uppercase the payload
    for (char* p = payload; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
    }

    memcpy(out.payload_string, payload, sizeof(out.payload_string) - 1);
    memcpy(out.material_type, mat_type, 6);
    out.color_hex = color;
    out.material_length_mm = len_mm;

    // Initial weight (sector 5)
    out.init_weight_g = spool.initial_weight_g;
    out.init_length_mm = TagData::weightToLength(spool.initial_weight_g);

    // Remaining (sectors 6-8 mirrors)
    out.remaining_weight_g = spool.current_weight_g;
    out.remaining_length_mm = TagData::weightToLength(spool.current_weight_g);
    out.update_counter = 1;

    // v2 extended fields (sectors 10-13)
    out.nozzle_temp_min = spool.nozzle_temp_min;
    out.nozzle_temp_max = spool.nozzle_temp_max;
    out.bed_temp_min = spool.bed_temp_min;
    out.bed_temp_max = spool.bed_temp_max;
    out.nozzle_temp_default = (spool.nozzle_temp_min + spool.nozzle_temp_max) / 2;
    out.bed_temp_default = (spool.bed_temp_min + spool.bed_temp_max) / 2;
    out.print_speed_min = spool.print_speed_min;
    out.print_speed_max = spool.print_speed_max;
    out.fan_percent = spool.fan_percent;
    out.diameter_um = spool.diameter_um;
    out.density_x100 = static_cast<uint16_t>(spool.density * 100);

    // Brand (12 chars max, null-padded)
    memset(out.brand, 0, sizeof(out.brand));
    size_t brand_len = spool.brand.length();
    if (brand_len > 12) brand_len = 12;
    memcpy(out.brand, spool.brand.c_str(), brand_len);

    // Product name (16 chars max, null-padded)
    memset(out.product_name, 0, sizeof(out.product_name));
    size_t name_len = spool.name.length();
    if (name_len > 16) name_len = 16;
    memcpy(out.product_name, spool.name.c_str(), name_len);
}

// ---------------------------------------------------------------------------
// TagData -> SpoolRecord: parse tag into inventory record
// ---------------------------------------------------------------------------
void spoolFromTagData(const TagData& tag, SpoolRecord& out) {
    // Don't clear — caller may have pre-set spool_id, etc.

    // UID
    out.tag_uid = tag.formatUID();

    // Material type (trimmed)
    String mt = tag.material_type;
    mt.trim();
    out.material_type = mt;

    // Color
    char hex_buf[8];
    snprintf(hex_buf, sizeof(hex_buf), "%06X", tag.color_hex & 0xFFFFFF);
    out.color_hex = hex_buf;

    // Weight from tag
    out.current_weight_g = tag.remaining_weight_g;
    out.initial_weight_g = tag.init_weight_g > 0 ? tag.init_weight_g : TagData::lengthToWeight(tag.material_length_mm);

    // Source
    out.source = SpoolSource::SCAN;

    // v2 extended fields
    if (tag.has_extended) {
        out.nozzle_temp_min = tag.nozzle_temp_min;
        out.nozzle_temp_max = tag.nozzle_temp_max;
        out.bed_temp_min = tag.bed_temp_min;
        out.bed_temp_max = tag.bed_temp_max;
        out.print_speed_min = tag.print_speed_min;
        out.print_speed_max = tag.print_speed_max;
        out.fan_percent = tag.fan_percent;
        out.diameter_um = tag.diameter_um;
        out.density = tag.density_x100 / 100.0f;
        out.brand = tag.brand;
        out.name = tag.product_name;
    } else {
        // v1: brand and name are not stored on tag
        out.brand = "";
        out.name = "";
        out.diameter_um = 1750;  // Default 1.75mm
    }

    uint32_t now = millis() / 1000;
    out.created_at = now;
    out.updated_at = now;
    out.status = SpoolStatus::ACTIVE;
}
