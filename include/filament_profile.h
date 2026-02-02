#pragma once
#include <Arduino.h>
#include <string>

struct FilamentProfile {
    String id;
    String brand;
    String name; // e.g., Hyper PLA
    String material_type; // e.g., PLA, PETG
    uint32_t color_hex;
    String color_name; // e.g., #FFFFFF
    uint16_t nozzle_temp;
    uint16_t bed_temp;
    uint32_t weight_g = 1000; // Default for new spools

    // Default constructor
    FilamentProfile() : id(""), brand(""), name(""), material_type(""), color_hex(0xFFFFFF), color_name("#FFFFFF"), nozzle_temp(0), bed_temp(0) {}

    // Constructor for convenience
    FilamentProfile(String id, String brand, String name, String type, uint32_t color, String colorStr, uint16_t nozzle, uint16_t bed)
        : id(id), brand(brand), name(name), material_type(type), color_hex(color), color_name(colorStr), nozzle_temp(nozzle), bed_temp(bed) {}
};