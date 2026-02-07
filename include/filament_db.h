// Copyright (c) 2026. Bradley Allan Corner
// Project: K2-RFID-CYD
//
// Use of this source code is governed by an MIT-style license that can be found in the LICENSE file or at https://opensource.org/licenses/MIT.
//

/**
 * @file filament_db.h
 * @brief In-memory filament catalog loaded from LittleFS (material_database.json).
 *
 * FilamentDB parses the Creality-style JSON (result.list[].base + kvParam),
 * builds a cache of FilamentProfile entries, and provides dropdown option
 * strings (brand, material type) for the UI. PSRAM-backed allocation is
 * used during load; the cache is held in a std::vector for the lifetime
 * of the app.
 */

#ifndef FILAMENT_DB_H
#define FILAMENT_DB_H

#include <ArduinoJson.h>
#include <vector>
#include <WString.h>
#include <filament_profile.h>

class FilamentDB {
public:
    FilamentDB();
    bool init();
    bool loadDatabase();
    std::vector<FilamentProfile> getAllFilaments();
    /** Returns profile by id; if not found, returns default-constructed profile (id will be empty). */
    FilamentProfile getProfileById(const String& id) const;
    /** Returns true if a profile with the given id exists and writes it to \a out; otherwise false. */
    bool getProfileById(const String& id, FilamentProfile& out) const;
    const std::vector<FilamentProfile>& getCache() const { return cache; }

    /** Newline-separated option strings for dropdowns (from material_database.json) */
    String getBrandOptionsForDropdown() const;
    String getMaterialTypeOptionsForDropdown() const;

private:
    std::vector<FilamentProfile> cache;
};

extern FilamentDB filamentDB;

#endif // FILAMENT_DB_H