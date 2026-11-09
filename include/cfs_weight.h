#pragma once
/**
 * @file cfs_weight.h
 * @brief Spool sizes the device writes into the CFS tag's length field.
 *
 * The 4-digit length field (payload pos 24-27) encodes the ORIGINAL spool size, never the
 * remaining amount (the K2 never writes tags). Known values from DnG-Crafts/K2-RFID v57
 * GetMaterialLength(); SimplyPrint documents the same set (800 g spool -> written as 750 g).
 * The device offers 250/500/750/1000 (Bradley, 2026-11-09). 600 g (0198) is a valid CFS value
 * that we don't write but still decode. 2 kg is untested on the K2 and left out on purpose.
 * See docs/rfid/tag-formats-landscape.md. Pure logic, host-testable (pio test -e native).
 */

#include <cstdint>
#include <cstddef>

namespace cfs {

struct WeightStep { uint32_t grams; uint32_t length; };

constexpr WeightStep kWeightSteps[] = {
    { 250, 82 }, { 500, 165 }, { 750, 247 }, { 1000, 330 },
};
constexpr size_t kWeightStepCount = sizeof(kWeightSteps) / sizeof(kWeightSteps[0]);
constexpr size_t kDefaultStepIndex = kWeightStepCount - 1;  // 1 kg, DnG's fallback

/// Index of the step to use for a spool of `grams`: the largest step not above it
/// (rounding down, as SimplyPrint does). Below 250 g -> 250 g. 0 = unknown -> 1 kg.
inline size_t weightStepIndex(uint32_t grams) {
    if (grams == 0) return kDefaultStepIndex;
    size_t idx = 0;
    for (size_t i = 0; i < kWeightStepCount; ++i) {
        if (kWeightSteps[i].grams <= grams) idx = i;
    }
    return idx;
}

/// Snap any weight to a valid CFS spool size.
inline uint32_t snapWeight(uint32_t grams) { return kWeightSteps[weightStepIndex(grams)].grams; }

/// Tag length value for a spool of `grams` (snapped first).
inline uint32_t weightToLength(uint32_t grams) { return kWeightSteps[weightStepIndex(grams)].length; }

/// Spool size from a tag length value. Known step values map exactly (0247 -> 750, not 748).
/// 0198 (600 g, a valid CFS value we don't offer) comes out exact through the formula.
/// Anything else (e.g. cards written by older firmware) falls back to the linear formula.
inline uint32_t lengthToWeight(uint32_t length) {
    for (size_t i = 0; i < kWeightStepCount; ++i) {
        if (kWeightSteps[i].length == length) return kWeightSteps[i].grams;
    }
    return 1000 * length / 330;
}

}  // namespace cfs
