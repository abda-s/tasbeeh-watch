#pragma once
#include <lvgl.h>

// ── Palette (from React design) ───────────────────────────────
// bg:      #0b1410  deep dark green-black
// surface: #0e2820  slightly lighter green
// gold:    #d4af37  antique gold (titles, active)
// ivory:   #f6e6b3  warm ivory (large time digits)
// cream:   #e9d9a8  soft cream (body text)
// cream50: #e9d9a8 @ 50%  dimmed text
// teal:    #7fd6a3  tasbeeh green ring
// border:  #d4af37 @ 10%  subtle gold border

extern lv_color_t color_bg;          // #0b1410
extern lv_color_t color_surface;     // #0e2820
extern lv_color_t color_gold;        // #d4af37
extern lv_color_t color_ivory;       // #f6e6b3
extern lv_color_t color_cream;       // #e9d9a8
extern lv_color_t color_cream_dim;   // muted cream
extern lv_color_t color_teal;        // #7fd6a3
extern lv_color_t color_border;      // dim gold border
extern lv_color_t color_red;         // battery warning
extern lv_color_t color_white;       // pure white (aliases ivory for compat)
extern lv_color_t color_grey;        // dim grey alias

// ── Styles ────────────────────────────────────────────────────
extern lv_style_t style_bg;
extern lv_style_t style_clock;        // large 48px time
extern lv_style_t style_title;        // screen title (gold, arabic)
extern lv_style_t style_label_sm;     // small label (cream dim)
extern lv_style_t style_label_ar;     // arabic body (cream)
extern lv_style_t style_label_ar_lg;  // large arabic (e.g. أستغفر الله)
extern lv_style_t style_counter_val;  // big counter number
extern lv_style_t style_arc_gold;     // istighfar arc (gold)
extern lv_style_t style_arc_teal;     // tasbeeh arc (teal)
extern lv_style_t style_pill_border;  // rounded border pill (date chip)
extern lv_style_t style_icon_btn;
extern lv_style_t style_btn_pill_teal;
extern lv_style_t style_btn_pill_blue;
extern lv_style_t style_card;

void ui_styles_init(void);
