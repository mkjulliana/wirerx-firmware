// =============================================================================
//  WireRx - ESP32-S3 + 2x ADS1015 + LVGL v8 + TFT_eSPI
//  SAFETY: switches 230 V AC. Use isolation (MOC3063), fusing, insulated enclosure,
//  and test with supervision. Add ~10k pulldowns on the MOC3063 LED drive lines.
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <math.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Adafruit_ADS1X15.h>
#include "driver/gpio.h"
#include "wirerx_ui.h"

// ============================ GPIO MAP (FINAL & SAFE) ========================
static constexpr int PIN_TRIAC_5A  = 1;    // 1 kW load
static constexpr int PIN_TRIAC_10A = 2;    // 2 kW load
static constexpr int PIN_PDC_CTRL  = 16;   // peak-detector MOSFETs
static constexpr int PIN_FAN       = 18;
static constexpr int PIN_BTN_START = 4;
static constexpr int PIN_BTN_LEFT  = 10;
static constexpr int PIN_BTN_RIGHT = 11;
static constexpr int PIN_BTN_OK    = 14;

static constexpr int  TRIAC_ON  = HIGH;
static constexpr int  PDC_RESET = HIGH;    // MOSFET on = discharge peak caps (flip if inverted)
static constexpr int  FAN_ON    = HIGH;
static constexpr bool START_ACTIVE_HIGH = true;   // schematic feeds START from 3.3 V

// ============================ ADS1015 (two I2C buses) ========================
static constexpr int I2C0_SDA = 12, I2C0_SCL = 17;   // voltage bus
static constexpr int I2C1_SDA = 8,  I2C1_SCL = 9;    // current bus
Adafruit_ADS1015 ads_voltage;   // A0 ZMPT101B, A1 +Vpeak
Adafruit_ADS1015 ads_current;   // A0 CT clamp,  A1 -Vpeak

// ============================ CALIBRATION (placeholders) =====================
static constexpr float V_SCALE      = 450.0f;  // mains V per ADC volt (AC part)
static constexpr float I_SCALE      = 30.0f;   // A per ADC volt (AC part)
static constexpr float PEAK_SCALE   = 109.1f;  // 0-3.3 V detector -> 0-360 Vpk
static constexpr float I_DEADBAND_A = 0.15f;
static constexpr int   BATTERY_PCT_PLACEHOLDER = 100;  // no battery-sense pin in schematic

// ============================ TIMING / THRESHOLDS ============================
static constexpr uint32_t LIVE_WINDOW_MS = 100;
static constexpr uint32_t TEST_WINDOW_MS = 200;
static constexpr uint32_t LIVE_PERIOD_MS = 500;
static constexpr uint32_t SETTLE_MS      = 150;
static constexpr uint32_t MAX_ON_MS      = 1000;   // hard failsafe
static constexpr uint32_t FAN_RUN_MS     = 30000;
static constexpr uint32_t DEBOUNCE_MS    = 40;
static_assert(MAX_ON_MS > SETTLE_MS + TEST_WINDOW_MS + 200, "MAX_ON_MS must exceed settle + sample window");

static constexpr float V_OVER_V    = 253.0f;
static constexpr float V_UNDER_V   = 207.0f;
static constexpr float VDROP_WARN  = 5.0f;
static constexpr float CREST_ARC   = 1.8f;
static constexpr float CREST_SPIKE = 1.556f;       // sine crest 1.414 x 1.10

// ============================ DISPLAY ========================================
static constexpr int SCREEN_W = 480, SCREEN_H = 320, BUF_LINES = 30;
TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_W * BUF_LINES];
static lv_color_t buf2[SCREEN_W * BUF_LINES];
static lv_disp_drv_t  disp_drv;
static lv_indev_drv_t indev_drv;
static uint16_t calData[5] = {300, 3600, 300, 3600, 1};   // run TFT_eSPI Touch_calibrate

static void disp_flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *c) {
    uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(a->x1, a->y1, w, h);
    tft.pushColors((uint16_t *)c, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(d);
}
static void touch_read(lv_indev_drv_t *, lv_indev_data_t *data) {
    uint16_t x, y;
    if (tft.getTouch(&x, &y, 300)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x; data->point.y = y;
    } else data->state = LV_INDEV_STATE_REL;
}

// ============================ STATE ==========================================
struct Meas { float vrms = 0, irms = 0, vpk = 0, pf = 1.0f, freq = 0; };
enum TestState { T_IDLE, T_LOADING };

static TestState g_state = T_IDLE;
static volatile uint8_t g_pending = 0;     // set by UI/buttons, consumed in loop
static uint8_t  g_selected_amps = 10;
static uint8_t  g_test_amps = 0;
static uint32_t g_t_on = 0;
static Meas     g_base, g_live;
static bool     g_hw_ok = true;
static bool     g_fan_on = false;
static uint32_t g_fan_off_at = 0;

// last test result (-1 = none yet, matches wirerx_ui.cpp conventions)
static float g_vdrop = -1, g_res = -1, g_conf = 0, g_pf_shown = 1.0f;
static int   g_fault = -1;

// ============================ PIN SAFETY =====================================
static bool pin_out_ok(int p) {
    if (p < 0 || (p >= 26 && p <= 32)) return false;    // WROOM-1 flash pins
    return GPIO_IS_VALID_OUTPUT_GPIO(p);
}
static bool pin_in_ok(int p) {
    if (p < 0 || (p >= 26 && p <= 32)) return false;
    return GPIO_IS_VALID_GPIO(p);
}
static void safe_write(int pin, int lvl) { if (pin_out_ok(pin)) digitalWrite(pin, lvl); }

static void relays_off() { safe_write(PIN_TRIAC_5A, !TRIAC_ON); safe_write(PIN_TRIAC_10A, !TRIAC_ON); }
static void fan_set(bool on) { safe_write(PIN_FAN, on ? FAN_ON : !FAN_ON); g_fan_on = on; }
static void pdc_reset() {
    if (!pin_out_ok(PIN_PDC_CTRL)) return;
    digitalWrite(PIN_PDC_CTRL, PDC_RESET);
    delay(2);
    digitalWrite(PIN_PDC_CTRL, !PDC_RESET);
}

// ============================ ADC SAMPLING ===================================
static void read_sample(float &v, float &i) {
    v = ads_voltage.computeVolts(ads_voltage.readADC_SingleEnded(0));   // ZMPT101B
    i = ads_current.computeVolts(ads_current.readADC_SingleEnded(0));   // CT clamp
}

static constexpr int MAX_N = 900;
static bool sample_window(uint32_t dur_ms, Meas &m) {
    static float vb[MAX_N], ib[MAX_N];
    static uint32_t tb[MAX_N];
    m = Meas{};
    pdc_reset();
    const uint32_t dur_us = dur_ms * 1000UL, t0 = micros();
    int n = 0;
    while (n < MAX_N) {
        uint32_t el = micros() - t0;
        if (el >= dur_us) break;
        read_sample(vb[n], ib[n]);
        tb[n] = el; n++;
    }
    if (n < 16) return false;

    double sv = 0, si = 0;
    for (int k = 0; k < n; k++) { sv += vb[k]; si += ib[k]; }
    const float mv = sv / n, mi = si / n;
    double svv = 0, sii = 0, svi = 0;
    for (int k = 0; k < n; k++) {
        float dv = vb[k] - mv, di = ib[k] - mi;
        svv += dv * dv; sii += di * di; svi += dv * di;
    }
    float vrms = sqrtf(svv / n) * V_SCALE;
    float irms = sqrtf(sii / n) * I_SCALE;
    float preal = (svi / n) * V_SCALE * I_SCALE;
    if (irms < I_DEADBAND_A) irms = 0;
    float pf = 1.0f;
    if (irms > 0 && vrms > 1.0f) pf = constrain(fabsf(preal) / (vrms * irms), 0.0f, 1.0f);

    // frequency from interpolated rising zero crossings (informational only)
    bool armed = false; int cross = 0; float tf = 0, tl = 0;
    for (int k = 1; k < n; k++) {
        float a = vb[k - 1] - mv, b = vb[k] - mv;
        if (b < -0.02f) armed = true;
        if (armed && a < 0 && b >= 0) {
            float t = tb[k - 1] + (-a / (b - a)) * (float)(tb[k] - tb[k - 1]);
            if (cross == 0) tf = t;
            tl = t; cross++; armed = false;
        }
    }
    float freq = (cross >= 3 && tl > tf) ? (cross - 1) * 1e6f / (tl - tf) : 0.0f;

    // peak detector: +Vpeak on ads_voltage A1, -Vpeak on ads_current A1
    float p = 0, q = 0;
    for (int k = 0; k < 3; k++) {
        p += ads_voltage.computeVolts(ads_voltage.readADC_SingleEnded(1));
        q += ads_current.computeVolts(ads_current.readADC_SingleEnded(1));
    }
    float vpk = fmaxf(p, q) / 3.0f * PEAK_SCALE;

    m.vrms = vrms; m.irms = irms; m.vpk = vpk; m.pf = pf; m.freq = freq;
    return true;
}

// ============================ CLASSIFIER (placeholder for TFLite) ============
static int classify_fault(float v_supply, float vdrop, float crest, float &conf) {
    if (crest > CREST_ARC && vdrop > VDROP_WARN) { conf = constrain(70 + (crest - CREST_ARC) * 40, 60, 99); return FAULT_ARC; }
    if (crest > CREST_SPIKE)  { conf = constrain(70 + (crest - CREST_SPIKE) * 60, 60, 99); return FAULT_TRANSIENT; }
    if (v_supply > V_OVER_V)  { conf = constrain(80 + (v_supply - V_OVER_V) * 1.5f, 60, 99); return FAULT_OVERVOLTAGE; }
    if (v_supply < V_UNDER_V) { conf = constrain(80 + (V_UNDER_V - v_supply) * 1.5f, 60, 99); return FAULT_UNDERVOLTAGE; }
    if (vdrop > VDROP_WARN)   { conf = constrain(75 + (vdrop - VDROP_WARN) * 5, 60, 99); return FAULT_WIRING_DEGRADATION; }
    conf = constrain(99 - vdrop * 4, 70, 99);
    return FAULT_NORMAL;
}

// ============================ UI HELPERS =====================================
static void ui_refresh(const Meas &m, float pf) {
    update_wirerx_display(m.vrms, m.irms, m.vpk, g_vdrop, g_res, g_fault, g_conf);
    wirerx_ui_set_pf(pf);
    wirerx_ui_set_status(BATTERY_PCT_PLACEHOLDER, false,
                         (WiFi.getMode() & WIFI_AP) != 0, g_state != T_IDLE);
}

static void on_ui_test(uint8_t amps) {        // runs inside lv_timer_handler()
    if (amps == 5 || amps == 10) g_pending = amps;
}

// ============================ LOAD TEST ======================================
static void start_test(uint8_t amps) {
    const int pin = (amps == 5) ? PIN_TRIAC_5A : PIN_TRIAC_10A;
    if (!g_hw_ok || !pin_out_ok(pin)) {
        wirerx_ui_log("%uA test refused: GPIO %d unusable / ADC missing", amps, pin);
        return;
    }
    if (!sample_window(TEST_WINDOW_MS, g_base)) { wirerx_ui_log("baseline sampling failed"); return; }
    g_test_amps = amps;
    fan_set(true);
    digitalWrite(pin, TRIAC_ON);
    g_t_on = millis();
    g_state = T_LOADING;
    wirerx_ui_log("[%lu] %uA test start, Vnl=%.1fV", millis() / 1000, amps, g_base.vrms);
    ui_refresh(g_live, g_pf_shown);           // shows TESTING + locks buttons
}

void test_tick() {
    if (g_state != T_LOADING) return;
    uint32_t el = millis() - g_t_on;

    if (el >= MAX_ON_MS) {                    // hard failsafe, checked first
        relays_off(); g_state = T_IDLE; g_fan_off_at = millis() + FAN_RUN_MS;
        wirerx_ui_log("ABORT: failsafe MAX_ON_MS hit");
        ui_refresh(g_live, g_pf_shown);
        return;
    }
    if (el < SETTLE_MS) return;               // still settling

    Meas ld;
    bool ok = sample_window(TEST_WINDOW_MS, ld);
    relays_off();                             // always open the relay first
    g_state = T_IDLE;
    g_fan_off_at = millis() + FAN_RUN_MS;

    if (!ok) { wirerx_ui_log("loaded sampling failed"); ui_refresh(g_live, g_pf_shown); return; }
    if (ld.irms < 0.3f * g_test_amps) {
        wirerx_ui_log("load not detected (%.2fA), discarded", ld.irms);
        ui_refresh(g_live, g_pf_shown);
        return;
    }

    float vnl = g_base.vrms;
    float dv  = fmaxf(0.0f, vnl - ld.vrms);
    g_vdrop = (vnl > 1.0f) ? dv / vnl * 100.0f : 0.0f;
    g_res   = (ld.irms > 0.5f) ? dv / ld.irms : -1.0f;
    float crest = (ld.vrms > 1.0f) ? ld.vpk / ld.vrms : 0.0f;
    g_fault = classify_fault(vnl, g_vdrop, crest, g_conf);
    g_live = ld; g_pf_shown = ld.pf;

    wirerx_ui_log("[%lu] done: Vld=%.1fV I=%.2fA drop=%.1f%% R=%.2f", millis() / 1000,
                  ld.vrms, ld.irms, g_vdrop, g_res);
    wirerx_ui_log("      class=%d conf=%.0f%% PF %.2f->%.2f f=%.1fHz",
                  g_fault, g_conf, g_base.pf, ld.pf, ld.freq);
    ui_refresh(ld, ld.pf);
}

// ============================ PHYSICAL BUTTONS ===============================
struct Btn { int pin; bool active_high; bool stable; bool last_raw; uint32_t t_change; };
static Btn b_start = {PIN_BTN_START, START_ACTIVE_HIGH, false, false, 0};
static Btn b_left  = {PIN_BTN_LEFT,  false, false, false, 0};
static Btn b_right = {PIN_BTN_RIGHT, false, false, false, 0};
static Btn b_ok    = {PIN_BTN_OK,    false, false, false, 0};

static void btn_init(Btn &b) {
    if (!pin_in_ok(b.pin)) { Serial.printf("[BTN] GPIO %d invalid on ESP32-S3, disabled\n", b.pin); return; }
    pinMode(b.pin, b.active_high ? INPUT_PULLDOWN : INPUT_PULLUP);
}
static bool btn_pressed(Btn &b) {        // true once per press
    if (!pin_in_ok(b.pin)) return false;
    bool raw = (digitalRead(b.pin) == (b.active_high ? HIGH : LOW));
    uint32_t now = millis();
    if (raw != b.last_raw) { b.last_raw = raw; b.t_change = now; }
    if (now - b.t_change >= DEBOUNCE_MS && raw != b.stable) { b.stable = raw; return raw; }
    return false;
}
static void poll_buttons() {
    if (btn_pressed(b_left))  { g_selected_amps = 5;  wirerx_ui_log("5A selected"); }
    if (btn_pressed(b_right)) { g_selected_amps = 10; wirerx_ui_log("10A selected"); }
    bool s = btn_pressed(b_start), o = btn_pressed(b_ok);
    if (s || o) g_pending = g_selected_amps;
}

// ============================ SETUP / LOOP ===================================
void setup() {
    // outputs to safe state first
    if (pin_out_ok(PIN_TRIAC_5A))  pinMode(PIN_TRIAC_5A,  OUTPUT);
    if (pin_out_ok(PIN_TRIAC_10A)) pinMode(PIN_TRIAC_10A, OUTPUT);
    if (pin_out_ok(PIN_PDC_CTRL))  pinMode(PIN_PDC_CTRL,  OUTPUT);
    if (pin_out_ok(PIN_FAN))       pinMode(PIN_FAN,       OUTPUT);
    relays_off();
    safe_write(PIN_PDC_CTRL, !PDC_RESET);
    fan_set(false);

    Serial.begin(115200);
    delay(200);

    bool pins_bad = !pin_out_ok(PIN_TRIAC_5A) || !pin_out_ok(PIN_TRIAC_10A);
    if (pins_bad) {
        g_hw_ok = false;
        Serial.printf("[HW] Triac GPIO %d/%d unusable on ESP32-S3-WROOM-1; tests LOCKED. Remap pins.\n",
                      PIN_TRIAC_5A, PIN_TRIAC_10A);
    }

    btn_init(b_start); btn_init(b_left); btn_init(b_right); btn_init(b_ok);

    // external ADCs: two independent I2C buses
    Wire.begin(I2C0_SDA, I2C0_SCL);
    Wire1.begin(I2C1_SDA, I2C1_SCL);
    bool a0 = ads_voltage.begin(0x48, &Wire);
    bool a1 = ads_current.begin(0x48, &Wire1);
    Wire.setClock(400000);
    Wire1.setClock(400000);
    if (!a0) Serial.println("[ADC] ads_voltage not found on Wire (SDA12/SCL17)");
    if (!a1) Serial.println("[ADC] ads_current not found on Wire1 (SDA8/SCL9)");
    if (!a0 || !a1) g_hw_ok = false;
    ads_voltage.setGain(GAIN_ONE);  ads_current.setGain(GAIN_ONE);        // +/-4.096 V
    ads_voltage.setDataRate(RATE_ADS1015_3300SPS);
    ads_current.setDataRate(RATE_ADS1015_3300SPS);

    // display + touch
    tft.begin();
    tft.setRotation(1);
    tft.setTouch(calData);
    tft.fillScreen(TFT_BLACK);

    lv_init();                                   // tick comes from millis() via lv_conf.h
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W; disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = disp_flush; disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("WireRx-AP");

    wirerx_ui_init();
    wirerx_ui_on_test(on_ui_test);
    wirerx_ui_log("WireRx boot OK, hw_ok=%d", g_hw_ok);
    if (pins_bad) wirerx_ui_log("WARNING: triac GPIOs invalid, tests locked");
    ui_refresh(g_live, g_pf_shown);
}

void loop() {
    static uint32_t last_live = 0;

    lv_timer_handler();
    test_tick();                       // failsafe first
    poll_buttons();

    if (g_pending) {
        uint8_t a = g_pending; g_pending = 0;
        if (g_state == T_IDLE) start_test(a);
    }
    if (g_fan_on && g_state == T_IDLE && (int32_t)(millis() - g_fan_off_at) >= 0) fan_set(false);

    if (g_state == T_IDLE && millis() - last_live >= LIVE_PERIOD_MS) {
        last_live = millis();
        Meas m;
        if (g_hw_ok && sample_window(LIVE_WINDOW_MS, m)) { g_live = m; ui_refresh(m, m.pf); }
        else ui_refresh(g_live, g_pf_shown);
    }
    delay(5);
}
