#pragma once
/**
 * @file material_db_store.h
 * @brief Where material_database.json lives: the SD card when one is mounted, else LittleFS.
 *
 * Writing ~430 KB to internal flash (LittleFS) stalls PSRAM, which holds the RGB panel's frame
 * buffer, and the screen slid diagonally for a few seconds on every Update DB (TODO.md,
 * 2026-11-09). SD writes go over SPI and don't. The LittleFS copy (uploadfs) stays as the
 * fallback for a board without a card; once the SD card has a copy, that one wins.
 */
#include <FS.h>

struct MaterialDbLocation {
    fs::FS* fs;
    const char* name;   // "SD" or "LittleFS", for logs
};

constexpr const char* MATERIAL_DB_PATH = "/material_database.json";
constexpr const char* MATERIAL_DB_TMP_PATH = "/material_database.json.tmp";

/// Copy to read: the SD card's if mounted and present, else LittleFS's.
MaterialDbLocation materialDbReadLocation();
/// Where Update DB writes: the SD card if mounted, else LittleFS.
MaterialDbLocation materialDbWriteLocation();
