#pragma once
#include <lvgl.h>

extern lv_color_t color_bg;
extern lv_color_t color_surface;
extern lv_color_t color_teal;
extern lv_color_t color_teal_dark;
extern lv_color_t color_blue;
extern lv_color_t color_gold;
extern lv_color_t color_green;
extern lv_color_t color_red;
extern lv_color_t color_white;
extern lv_color_t color_grey;
extern lv_color_t color_border;

extern lv_style_t style_bg;
extern lv_style_t style_clock;
extern lv_style_t style_title;
extern lv_style_t style_label_sm;
extern lv_style_t style_label_ar;
extern lv_style_t style_btn_pill_teal;
extern lv_style_t style_btn_pill_blue;
extern lv_style_t style_counter_val;
extern lv_style_t style_arc_teal;
extern lv_style_t style_arc_blue;
extern lv_style_t style_card;
extern lv_style_t style_icon_btn;

void ui_styles_init(void);
