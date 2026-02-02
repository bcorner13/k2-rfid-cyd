#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>
#include <algorithm>
#include <vector>
#include "filament_profile.h"

// Helper to format strings
inline std::string format_hex(uint32_t val, int digits) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%0*X", digits, val);
    return std::string(buf);
}

struct SpoolData {
  public:
    SpoolData() = default;

    // Constructor from FilamentProfile (for writing to tag)
    explicit SpoolData(const FilamentProfile& profile) {
        _brandName = profile.brand.c_str(); // Store brand name
        _materialType = profile.material_type.c_str();
        _materialColorNumeric = profile.color_hex;
        _materialColorString = profile.color_name.c_str();
        _materialWeight = profile.weight_g;

        // Defaults for new tags
        _materialVendor = "0276"; // Creality Default
        _materialBatch = "A2";
        _materialDate = "AB124";
        _reserve = "000000";
        _serialNum = std::to_string(random(100000, 999999));

        _generateSpooldataString();
    }

    // Constructor from Raw RFID String (Reading from Tag)
    explicit SpoolData(const std::string& spooldata_in) {
        std::string spooldata = spooldata_in;

        size_t last_numeric_pos = spooldata.find_last_of("0123456789");
        if (last_numeric_pos != std::string::npos) {
            spooldata.erase(last_numeric_pos + 1);
        } else {
            spooldata.clear();
        }

        if (spooldata.length() < 34) {
            _spooldata = "";
            return;
        }

        _materialDate = spooldata.substr(0, 5);
        _materialVendor = spooldata.substr(5, 4);
        _materialBatch = spooldata.substr(9, 2);
        _materialType = spooldata.substr(12, 5);

        try {
            _materialColorNumeric = std::stoi(spooldata.substr(18, 6), nullptr, 16);
        } catch (...) { _materialColorNumeric = 0; }

        char col_str[8];
        snprintf(col_str, sizeof(col_str), "#%06X", _materialColorNumeric);
        _materialColorString = std::string(col_str);

        try {
            int len = std::stoi(spooldata.substr(24, 4));
            _materialWeight = _convertMaterialWeight(len);
        } catch (...) { _materialWeight = 0; }

        _serialNum = spooldata.substr(28, 6);
        if (spooldata.length() > 34) {
            _reserve = spooldata.substr(34);
        } else {
            _reserve = "";
        }

        _spooldata = spooldata;
    }

    // Getters
    std::string getRawData() const { return _spooldata; }
    std::string getType() const { return _materialType; }
    std::string getBrand() const { return _brandName; } // Added brand getter
    std::string getColorName() const { return _materialColorString; }
    uint32_t getColorHex() const { return _materialColorNumeric; }
    uint32_t getWeight() const { return _materialWeight; }

    // Setters
    void setWeight(uint32_t w) {
        _materialWeight = w;
        _generateSpooldataString();
    }

    void setType(const char* t) {
        _materialType = t;
        _generateSpooldataString();
    }

    void setColor(uint32_t hex) {
        _materialColorNumeric = hex;
        // Update the string representation too!
        char col_str[8];
        snprintf(col_str, sizeof(col_str), "#%06X", _materialColorNumeric);
        _materialColorString = std::string(col_str);

        _generateSpooldataString();
    }

    // Generate the raw string for writing
    void _generateSpooldataString() {
      _spooldata = "";
      _spooldata += _materialDate;
      _spooldata += _materialVendor;
      _spooldata += _materialBatch;
      _spooldata += "1";

      std::string type = _materialType;
      while(type.length() < 5) type += " ";
      if(type.length() > 5) type = type.substr(0,5);
      _spooldata += type;

      char col_str[8];
      snprintf(col_str, sizeof(col_str), "0%06X", _materialColorNumeric);
      _spooldata += col_str;

      uint32_t len = _convertMaterialLength(_materialWeight);
      char len_str[8];
      snprintf(len_str, sizeof(len_str), "%04d", len);
      _spooldata += len_str;

      _spooldata += _serialNum;

      std::transform(_spooldata.begin(), _spooldata.end(), _spooldata.begin(), ::toupper);
    }

  private:
    std::string _spooldata = "";
    std::string _materialBatch = "";
    std::string _materialDate = "";
    std::string _materialVendor = "";
    std::string _materialType = "";
    std::string _brandName = ""; // Added brand name member
    uint32_t _materialWeight = 0;
    uint32_t _materialColorNumeric = 0;
    std::string _materialColorString = "";
    std::string _serialNum = "";
    std::string _reserve = "";

    uint32_t _convertMaterialLength(const uint32_t& weight) {
      return 330 * weight / 1000;
    }

    uint32_t _convertMaterialWeight(const uint32_t& length) {
      return 1000 * length / 330;
    }
};