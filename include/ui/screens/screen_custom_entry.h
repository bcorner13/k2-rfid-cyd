#pragma once
#include <lvgl.h>
#include <Arduino.h>

class ScreenCustomEntry {
public:
    void init();
    void show();
    void reset();  // Clear form for fresh entry

    lv_obj_t* screen;

    // Navigation buttons (public for UIManager event wiring)
    lv_obj_t* btnCancel;
    lv_obj_t* btnPrev;
    lv_obj_t* btnNext;
    lv_obj_t* btnSave;
    lv_obj_t* btnSaveWrite;

    // Step 1 inputs
    lv_obj_t* taBrand;
    lv_obj_t* taName;
    lv_obj_t* ddMaterial;

    // Step 2 inputs
    lv_obj_t* colorSwatch;
    lv_obj_t* ddDiameter;
    uint32_t selectedColor = 0xFFFFFF;

    // Step 3 inputs
    lv_obj_t* spinWeight;
    lv_obj_t* spinNozzleMin;
    lv_obj_t* spinNozzleMax;
    lv_obj_t* spinBedMin;
    lv_obj_t* spinBedMax;

    // Step 4 inputs
    lv_obj_t* spinSpeedMin;
    lv_obj_t* spinSpeedMax;
    lv_obj_t* sliderFan;
    lv_obj_t* labelFanVal;

    uint8_t currentStep = 0;  // 0-4

    void goToStep(uint8_t step);

private:
    static constexpr uint8_t STEP_COUNT = 5;
    lv_obj_t* stepPanels[STEP_COUNT];
    lv_obj_t* stepIndicator;
    lv_obj_t* labelStepTitle;

    void createStep1();
    void createStep2();
    void createStep3();
    void createStep4();
    void createStep5();
    void populateReview();

    // Color picker modal
    lv_obj_t* modalColor;
    void createColorPicker();

    // Step 5 review labels
    lv_obj_t* lblReviewBrand;
    lv_obj_t* lblReviewName;
    lv_obj_t* lblReviewMaterial;
    lv_obj_t* lblReviewColor;
    lv_obj_t* lblReviewDiameter;
    lv_obj_t* lblReviewWeight;
    lv_obj_t* lblReviewNozzle;
    lv_obj_t* lblReviewBed;
    lv_obj_t* lblReviewSpeed;
    lv_obj_t* lblReviewFan;
};

extern ScreenCustomEntry screenCustomEntry;
