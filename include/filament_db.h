// Copyright (c) 2026. Bradley Allan Corner
// Project: K2-RFID-CYD
//
// Use of this source code is governed by an MIT-style license that can be found in the LICENSE file or at https://opensource.org/licenses/MIT.
//

#ifndef FILAMENT_DB_H
#define FILAMENT_DB_H

#include <ArduinoJson.h> // For String, etc.
#include <vector>
#include <WString.h> // For String
#include <filament_profile.h> // Include the header where FilamentProfile is defined

// Removed the redefinition of struct FilamentProfile

class FilamentDB {
public:
    FilamentDB();
    bool init();
    bool loadDatabase();
    std::vector<FilamentProfile> getAllFilaments();
    FilamentProfile getProfileById(const String& id) const;
    const std::vector<FilamentProfile>& getCache() const { return cache; }

private:
    std::vector<FilamentProfile> cache;
};

extern FilamentDB filamentDB;

#endif // FILAMENT_DB_H