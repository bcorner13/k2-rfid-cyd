#pragma once
/**
 * @file dropdown_match.h
 * @brief Index of an option in an LVGL dropdown option string ("A\nB\nC"). Pure; host-tested.
 *
 * An exact (space-trimmed) match always wins. A prefix match is used only when no option
 * matches exactly: before, the first option starting with the target won, so "Hyper PLA"
 * could select "Hyper PLA Matte" if that came first. No match returns 0.
 */
#include <cstdint>
#include <string>

inline uint16_t findDropdownOption(const char* options, const std::string& target) {
    if (!options || target.empty()) return 0;
    const std::string all(options);
    bool havePrefix = false;
    uint16_t prefixIdx = 0, index = 0;
    size_t start = 0;
    while (start <= all.length()) {
        size_t end = all.find('\n', start);
        if (end == std::string::npos) end = all.length();
        const size_t first = all.find_first_not_of(' ', start);
        std::string opt;
        if (first != std::string::npos && first < end) {
            const size_t last = all.find_last_not_of(' ', end - 1);
            opt = all.substr(first, last - first + 1);
        }
        if (opt == target) return index;
        if (!havePrefix && opt.compare(0, target.length(), target) == 0) {
            havePrefix = true;
            prefixIdx = index;
        }
        if (end == all.length()) break;
        start = end + 1;
        index++;
    }
    return havePrefix ? prefixIdx : 0;
}
