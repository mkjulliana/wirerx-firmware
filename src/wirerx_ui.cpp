#include "wirerx_ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Arduino.h>

/* ---------------- geometry (480 x 320) ---------------- */
static const int HDR_H   = 35;
static const int CARD_W  = 152, CARD_H = 86;
static const int COL_X[3] = {6, 164, 322};
static const int ROW_Y[2] = {39, 129};
static const int BAN_Y   = 222, BAN_H = 50;
static const int BTN_Y   = 276, BTN_H = 42, BTN_W = 152;

/* ---------------- palette ---------------- */
#define C_BG      lv_color_hex(0x0F172A)
#define C_PANEL   lv_color_hex(0x1E293B)
#define C_BORDER  lv_color_hex(0x334155)
#define C_TEXT    lv_color_hex(0xF8FAFC)
#define C_MUTED   lv_color_hex(0x94A3B8)
#define C_GREEN   lv_color_hex(0x22C55E)
#define C_YELLOW  lv_color_hex(0xFACC15)
#define C_RED     lv_color_hex(0xEF4444)

struct FaultStyle { const char *name; uint32_t color; };
static const FaultStyle FAULTS[FAULT_COUNT] = {
    {"NORMAL",             0x22C55E},
    {"WIRING DEGRADATION", 0xF59E0B},
    {"ARC FAULT",          0xEF4444},
    {"OVERVOLTAGE",        0xF97316},
    {"UNDERVOLTAGE",       0x38BDF8},
    {"TRANSIENT SPIKE",    0xA78BFA},
};

/* ---------------- widget handles ---------------- */
static lv_obj_t *scr_main, *scr_log;
static lv_obj_t *lbl_batt, *lbl_wifi, *lbl_state;
static lv_obj_t *card_vdrop, *lbl_vrms, *lbl_irms, *lbl_vdrop, *lbl_badge,
                *lbl_res, *lbl_cf, *lbl_pf;
static lv_obj_t *ban, *lbl_fault, *lbl_conf, *bar_conf;
static lv_obj_t *btn_5a, *btn_10a;
static lv_obj_t *ta_log;
static wirerx_test_cb_t s_test_cb = nullptr;

/* ---------------- helpers ---------------- */
static lv_obj_t *make_plain(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(o, 8, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t col) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    return l;
}

/* One data card: small title on top, big value below. Returns the card, value label via out param. */
static lv_obj_t *make_card(lv_obj_t *parent, int col, int row, const char *title,
                           lv_obj_t **value_out, lv_color_t value_col) {
    lv_obj_t *c = make_plain(parent, COL_X[col], ROW_Y[row], CARD_W, CARD_H);
    lv_obj_set_style_bg_color(c, C_PANEL, 0);
    lv_obj_set_style_border_color(c, C_BORDER, 0);
    lv_obj_set_style_border_width(c, 2, 0);

    lv_obj_t *t = make_label(c, title, &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *v = make_label(c, "--", &lv_font_montserrat_28, value_col);
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 8, -10);
    *value_out = v;
    return c;
}

static void test_btn_cb(lv_event_t *e) {
    uint8_t amps = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_test_cb) s_test_cb(amps);
}

static void logs_open_cb(lv_event_t *) { lv_scr_load(scr_log); }
static void logs_back_cb(lv_event_t *) { lv_scr_load(scr_main); }

static lv_obj_t *make_button(lv_obj_t *parent, int x, const char *txt, uint32_t color,
                             lv_event_cb_t cb, void *user) {
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, BTN_W, BTN_H);
    lv_obj_set_pos(b, x, BTN_Y);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x475569), LV_STATE_DISABLED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x0F172A), 0);
    lv_obj_center(l);
    return b;
}

/* ---------------- builders ---------------- */
static void build_header(lv_obj_t *scr) {
    lv_obj_t *h = make_plain(scr, 0, 0, 480, HDR_H);
    lv_obj_set_style_radius(h, 0, 0);
    lv_obj_set_style_bg_color(h, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(h, 0, 0);

    lv_obj_t *t = make_label(h, "WireRx 230V", &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 8, 0);

    lbl_state = make_label(h, "IDLE", &lv_font_montserrat_14, C_MUTED);
    lv_obj_set_style_bg_color(lbl_state, C_PANEL, 0);
    lv_obj_set_style_bg_opa(lbl_state, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(lbl_state, 8, 0);
    lv_obj_set_style_pad_ver(lbl_state, 2, 0);
    lv_obj_set_style_radius(lbl_state, 4, 0);
    lv_obj_align(lbl_state, LV_ALIGN_CENTER, 20, 0);

    lbl_wifi = make_label(h, LV_SYMBOL_WIFI, &lv_font_montserrat_16, C_MUTED);
    lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -92, 0);

    lbl_batt = make_label(h, LV_SYMBOL_BATTERY_FULL " --%", &lv_font_montserrat_14, C_TEXT);
    lv_obj_align(lbl_batt, LV_ALIGN_RIGHT_MID, -8, 0);
}

static void build_grid(lv_obj_t *scr) {
    make_card(scr, 0, 0, "Vrms", &lbl_vrms, lv_color_hex(0xFDE047));
    make_card(scr, 1, 0, "Irms", &lbl_irms, lv_color_hex(0x67E8F9));

    card_vdrop = make_card(scr, 2, 0, "V DROP (PEC)", &lbl_vdrop, C_TEXT);
    lbl_badge = make_label(card_vdrop, "--", &lv_font_montserrat_12, lv_color_hex(0x0F172A));
    lv_obj_set_style_bg_color(lbl_badge, C_BORDER, 0);
    lv_obj_set_style_bg_opa(lbl_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(lbl_badge, 6, 0);
    lv_obj_set_style_radius(lbl_badge, 4, 0);
    lv_obj_align(lbl_badge, LV_ALIGN_TOP_RIGHT, -6, 4);

    make_card(scr, 0, 1, "dV / I RESISTANCE", &lbl_res, C_TEXT);
    make_card(scr, 1, 1, "CREST FACTOR", &lbl_cf, C_TEXT);
    make_card(scr, 2, 1, "POWER FACTOR", &lbl_pf, C_TEXT);
}

static void build_banner(lv_obj_t *scr) {
    ban = make_plain(scr, 6, BAN_Y, 468, BAN_H);
    lv_obj_set_style_bg_color(ban, C_PANEL, 0);
    lv_obj_set_style_border_width(ban, 3, 0);
    lv_obj_set_style_border_color(ban, C_BORDER, 0);

    lbl_fault = make_label(ban, "AWAITING TEST", &lv_font_montserrat_20, C_MUTED);
    lv_obj_align(lbl_fault, LV_ALIGN_TOP_LEFT, 10, 4);

    lbl_conf = make_label(ban, "AI --%", &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(lbl_conf, LV_ALIGN_TOP_RIGHT, -10, 4);

    bar_conf = lv_bar_create(ban);
    lv_obj_set_size(bar_conf, 440, 8);
    lv_obj_align(bar_conf, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_bar_set_range(bar_conf, 0, 100);
    lv_bar_set_value(bar_conf, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_conf, C_BORDER, LV_PART_MAIN);
}

static void build_buttons(lv_obj_t *scr) {
    btn_5a  = make_button(scr, COL_X[0], "5A TEST",  0x38BDF8, test_btn_cb, (void *)(uintptr_t)5);
    btn_10a = make_button(scr, COL_X[1], "10A TEST", 0x22C55E, test_btn_cb, (void *)(uintptr_t)10);
    make_button(scr, COL_X[2], LV_SYMBOL_LIST " LOGS", 0xCBD5E1, logs_open_cb, nullptr);
}

static void build_log_screen() {
    scr_log = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_log, C_BG, 0);
    lv_obj_clear_flag(scr_log, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = make_label(scr_log, "EVENT LOG", &lv_font_montserrat_16, C_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 8);

    ta_log = lv_textarea_create(scr_log);
    lv_obj_set_size(ta_log, 468, 220);
    lv_obj_set_pos(ta_log, 6, 34);
    lv_textarea_set_text(ta_log, "");
    lv_obj_set_style_bg_color(ta_log, lv_color_hex(0x020617), 0);
    lv_obj_set_style_text_color(ta_log, lv_color_hex(0x86EFAC), 0);
    lv_obj_set_style_text_font(ta_log, &lv_font_montserrat_12, 0);
    lv_obj_clear_flag(ta_log, LV_OBJ_FLAG_CLICK_FOCUSABLE);   // read-only: no keyboard/cursor focus

    lv_obj_t *back = lv_btn_create(scr_log);
    lv_obj_set_size(back, 152, BTN_H);
    lv_obj_set_pos(back, 164, BTN_Y);
    lv_obj_add_event_cb(back, logs_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);
}

/* ---------------- public API ---------------- */
void wirerx_ui_init() {
    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, C_BG, 0);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr_main);
    build_grid(scr_main);
    build_banner(scr_main);
    build_buttons(scr_main);
    build_log_screen();

    lv_scr_load(scr_main);
}

void wirerx_ui_on_test(wirerx_test_cb_t cb) { s_test_cb = cb; }

void update_wirerx_display(float vrms, float irms, float vpk,
                           float vdrop, float resistance,
                           int fault_class, float confidence) {
    char buf[32];

    /* NOTE: LVGL's built-in printf has no %f, so format floats with snprintf first. */
    snprintf(buf, sizeof buf, "%.1f V", vrms);  lv_label_set_text(lbl_vrms, buf);
    snprintf(buf, sizeof buf, "%.2f A", irms);  lv_label_set_text(lbl_irms, buf);

    float cf = (vrms > 1.0f) ? vpk / vrms : 0.0f;
    snprintf(buf, sizeof buf, "%.2f", cf);
    lv_label_set_text(lbl_cf, buf);
    lv_obj_set_style_text_color(lbl_cf, cf > 1.8f ? C_RED : C_TEXT, 0);   // arc threshold from thesis Table 3

    /* Vdrop with PEC colour coding: <=5 green, 5-8 yellow, >8 red */
    if (vdrop < 0.0f) {
        lv_label_set_text(lbl_vdrop, "-- %");
        lv_label_set_text(lbl_badge, "--");
        lv_obj_set_style_text_color(lbl_vdrop, C_TEXT, 0);
        lv_obj_set_style_bg_color(lbl_badge, C_BORDER, 0);
        lv_obj_set_style_border_color(card_vdrop, C_BORDER, 0);
    } else {
        lv_color_t c; const char *tag;
        if (vdrop <= 5.0f)      { c = C_GREEN;  tag = "PEC OK"; }
        else if (vdrop <= 8.0f) { c = C_YELLOW; tag = "WEAK"; }
        else                    { c = C_RED;    tag = "CRITICAL"; }
        snprintf(buf, sizeof buf, "%.1f %%", vdrop);
        lv_label_set_text(lbl_vdrop, buf);
        lv_label_set_text(lbl_badge, tag);
        lv_obj_set_style_text_color(lbl_vdrop, c, 0);
        lv_obj_set_style_bg_color(lbl_badge, c, 0);
        lv_obj_set_style_border_color(card_vdrop, c, 0);
    }

    /* The Montserrat fonts have no Omega glyph, so the unit is spelled out. */
    if (resistance < 0.0f) lv_label_set_text(lbl_res, "-- ohm");
    else { snprintf(buf, sizeof buf, "%.2f ohm", resistance); lv_label_set_text(lbl_res, buf); }

    /* AI banner */
    if (fault_class < 0 || fault_class >= FAULT_COUNT) {
        lv_label_set_text(lbl_fault, "AWAITING TEST");
        lv_obj_set_style_text_color(lbl_fault, C_MUTED, 0);
        lv_obj_set_style_border_color(ban, C_BORDER, 0);
        lv_label_set_text(lbl_conf, "AI --%");
        lv_bar_set_value(bar_conf, 0, LV_ANIM_OFF);
    } else {
        lv_color_t fc = lv_color_hex(FAULTS[fault_class].color);
        lv_label_set_text(lbl_fault, FAULTS[fault_class].name);
        lv_obj_set_style_text_color(lbl_fault, fc, 0);
        lv_obj_set_style_border_color(ban, fc, 0);
        lv_obj_set_style_bg_color(bar_conf, fc, LV_PART_INDICATOR);
        if (confidence < 0) confidence = 0;
        if (confidence > 100) confidence = 100;
        snprintf(buf, sizeof buf, "AI %.0f%%", confidence);
        lv_label_set_text(lbl_conf, buf);
        lv_bar_set_value(bar_conf, (int32_t)confidence, LV_ANIM_OFF);
    }
}

void wirerx_ui_set_pf(float pf) {
    char buf[16];
    snprintf(buf, sizeof buf, "%.2f", pf);
    lv_label_set_text(lbl_pf, buf);
    lv_obj_set_style_text_color(lbl_pf, pf >= 0.90f ? C_TEXT : C_YELLOW, 0);   // healthy PF >= 0.90 per thesis
}

void wirerx_ui_set_status(int battery_pct, bool charging, bool wifi_ok, bool testing) {
    const char *sym = battery_pct > 85 ? LV_SYMBOL_BATTERY_FULL
                    : battery_pct > 60 ? LV_SYMBOL_BATTERY_3
                    : battery_pct > 35 ? LV_SYMBOL_BATTERY_2
                    : battery_pct > 10 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
    char buf[32];
    snprintf(buf, sizeof buf, "%s%s %d%%", charging ? LV_SYMBOL_CHARGE : "", sym, battery_pct);
    lv_label_set_text(lbl_batt, buf);
    lv_obj_set_style_text_color(lbl_batt, battery_pct <= 20 ? C_RED : C_TEXT, 0);

    lv_obj_set_style_text_color(lbl_wifi, wifi_ok ? lv_color_hex(0x38BDF8) : C_MUTED, 0);

    lv_label_set_text(lbl_state, testing ? "TESTING" : "IDLE");
    lv_obj_set_style_bg_color(lbl_state, testing ? lv_color_hex(0xF59E0B) : C_PANEL, 0);
    lv_obj_set_style_text_color(lbl_state, testing ? lv_color_hex(0x0F172A) : C_MUTED, 0);

    /* lock the test buttons while a load test is running */
    if (testing) { lv_obj_add_state(btn_5a, LV_STATE_DISABLED);  lv_obj_add_state(btn_10a, LV_STATE_DISABLED); }
    else         { lv_obj_clear_state(btn_5a, LV_STATE_DISABLED); lv_obj_clear_state(btn_10a, LV_STATE_DISABLED); }
}

void wirerx_ui_log(const char *fmt, ...) {
    char line[96];
    va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof line - 2, fmt, ap);
    va_end(ap);
    size_t n = strlen(line);
    line[n] = '\n'; line[n + 1] = 0;
    lv_textarea_add_text(ta_log, line);
    Serial.print(line);
}
