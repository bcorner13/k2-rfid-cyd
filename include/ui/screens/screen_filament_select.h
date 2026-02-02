// Copyright (c) 2026. Bradley Allan Corner
// Project: K2-RFID-CYD
//
// Use of this source code is governed by an MIT-style license that can be found in the LICENSE file or at https://opensource.org/licenses/MIT.
//

#ifndef SCREEN_FILAMENT_SELECT_H
#define SCREEN_FILAMENT_SELECT_H

#include <lvgl.h>

// Define ScreenFilamentSelect as a class
class ScreenFilamentSelect {
public:
    void init();
    void show();

private:
    lv_obj_t* screen; // Now a member variable
    lv_obj_t* cont;   // Now a member variable

    // Static constants can remain static or be made const members if appropriate
    static const uint8_t FILAMENT_COLUMNS = 2;
    static const uint16_t BUTTON_HEIGHT = 60;
};

// Declare the global instance of ScreenFilamentSelect
extern ScreenFilamentSelect screenFilamentSelect;

#endif // SCREEN_FILAMENT_SELECT_H