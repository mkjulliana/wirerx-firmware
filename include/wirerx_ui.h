#pragma once
/*
 * WireRx UI  -  LVGL v8 screen for a 480x320 landscape TFT
 * Layout: header 35px | 2x3 data grid | AI banner 50px | buttons ~45px
 */
#include <lvgl.h>
#include <stdint.h>

/* Order matches the 6 WireRx fault classes (use as model output index). */
enum WireRxFault : int {
    FAULT_NORMAL = 0,
    FAULT_WIRING_DEGRADATION,
    FAULT_ARC,
    FAULT_OVERVOLTAGE,
    FAULT_UNDERVOLTAGE,
    FAULT_TRANSIENT,
    FAULT_COUNT
};

/* Called from the LVGL context when 5A TEST / 10A TEST is tapped. */
typedef void (*wirerx_test_cb_t)(uint8_t amps);

void wirerx_ui_init();                                   // build all screens
void wirerx_ui_on_test(wirerx_test_cb_t cb);             // register button handler

/* Redraw live labels. vdrop < 0 or resistance < 0 means "no test result yet" (shows "--").
 * Crest factor is derived inside as vpk / vrms. confidence is 0..100 (%). */
void update_wirerx_display(float vrms, float irms, float vpk,
                           float vdrop, float resistance,
                           int fault_class, float confidence);

void wirerx_ui_set_pf(float pf);                         // Power Factor card
void wirerx_ui_set_status(int battery_pct, bool charging,
                          bool wifi_ok, bool testing);   // header bar
void wirerx_ui_log(const char *fmt, ...);                // append a line to the LOGS screen
