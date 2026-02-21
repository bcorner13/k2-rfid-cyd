#pragma once
#include <lvgl.h>
#include <Arduino.h>

struct SpoolRecord;  // Forward declaration

class ScreenSpoolDetail {
public:
    void init();
    void show();

    /** Load spool data into the screen. Call before show(). */
    void loadSpool(const String& spool_id);

    lv_obj_t* screen;
    lv_obj_t* btnBack;
    lv_obj_t* btnUpdateWeight;
    lv_obj_t* btnWriteTag;
    lv_obj_t* btnUnlinkTag;
    lv_obj_t* btnArchive;
    lv_obj_t* btnDelete;
    lv_obj_t* btnWeightOk;
    lv_obj_t* btnWeightCancel;
    lv_obj_t* modalWeight;
    lv_obj_t* spinboxWeight;

    String currentSpoolId;  // ID of the spool currently displayed

private:
    // Header
    lv_obj_t* colorSwatch;
    lv_obj_t* labelName;
    lv_obj_t* labelMaterial;
    lv_obj_t* labelTagUID;

    // Info section
    lv_obj_t* labelNozzleTemp;
    lv_obj_t* labelBedTemp;
    lv_obj_t* labelSpeed;
    lv_obj_t* labelFan;
    lv_obj_t* labelDiameter;
    lv_obj_t* labelDensity;

    // Weight section
    lv_obj_t* barWeight;
    lv_obj_t* labelWeightPct;
    lv_obj_t* labelWeightText;

    // Weight history
    lv_obj_t* historyList;

    void createWeightModal();
    void _lazyInit();
    bool _initialized = false;
};

extern ScreenSpoolDetail screenSpoolDetail;
