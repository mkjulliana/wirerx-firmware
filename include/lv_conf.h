/* Minimal LVGL v8.3 config for WireRx. Anything not defined here falls back
 * to the defaults in lv_conf_internal.h. */
#if 1 /* enable this file */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color: RGB565. Byte swap is done in the flush callback (pushColors swap=true) */
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0

/* Use the C heap (ESP32 malloc) instead of LVGL's fixed pool */
#define LV_MEM_CUSTOM      1

/* Millisecond tick from Arduino, so no manual lv_tick_inc() is needed */
#define LV_TICK_CUSTOM                 1
#define LV_TICK_CUSTOM_INCLUDE         "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR   (millis())

#define LV_DISP_DEF_REFR_PERIOD  30
#define LV_INDEV_DEF_READ_PERIOD 30

/* Fonts used by the UI */
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

#define LV_USE_LOG 0

#endif /*LV_CONF_H*/
#endif
