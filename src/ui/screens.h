#pragma once
#include <lvgl.h>

extern lv_obj_t *scr_home;
extern lv_obj_t *scr_thiker;
extern lv_obj_t *scr_tasbeeh;
extern lv_obj_t *scr_isteghfar;
extern lv_obj_t *scr_settings;
extern lv_obj_t *scr_timeedit;

extern lv_obj_t *home_clock_label;
extern lv_obj_t *home_date_label;
extern lv_obj_t *home_bat_label;
extern lv_obj_t *tasbeeh_arc;
extern lv_obj_t *tasbeeh_counter_label;
extern lv_obj_t *tasbeeh_total_label;
extern lv_obj_t *isteghfar_arc;
extern lv_obj_t *isteghfar_counter_label;
extern lv_obj_t *isteghfar_total_label;
extern lv_obj_t *settings_ip_label;

void screens_init(void);
void update_home_clock(void);
void update_home_battery(void);
void update_tasbeeh_display(void);
void update_isteghfar_display(void);
