#include "screens.h"
#include "styles.h"
#include "config/reminders_config.h"
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);

// Row count tracks REMINDER_PRESETS (config/reminders_config.h) directly —
// add/remove a preset there and this list follows, no separate count to keep in sync.
#define NOTIF_ROWS REMINDER_PRESET_COUNT
#define NOTIF_ROW_H 34

static lv_obj_t *notif_time_labels[NOTIF_ROWS];
static lv_obj_t *notif_switches[NOTIF_ROWS];

static void notif_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_notifications) return;
    pop_modal();
}

static void notif_switch_cb(lv_event_t *e) {
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    reminders[idx].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    saveReminders();
}

static void notif_row_cb(lv_event_t *e) {
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    open_timeedit_reminder(idx);
}

void refresh_notifications_list(void) {
    for (int i = 0; i < NOTIF_ROWS; i++) {
        if (!notif_time_labels[i]) continue;
        lv_label_set_text_fmt(notif_time_labels[i], "%02d:%02d",
            reminders[i].hour, reminders[i].minute);
        if (reminders[i].enabled)
            lv_obj_add_state(notif_switches[i], LV_STATE_CHECKED);
        else
            lv_obj_remove_state(notif_switches[i], LV_STATE_CHECKED);
    }
}

void create_screen_notifications(void) {
    scr_notifications = lv_obj_create(NULL);
    make_screen_base(scr_notifications);

    // Compact header (back-arrow + title in one row) — identical footprint to
    // screen_timeedit.cpp's header (180x36 at ALIGN_CENTER(0,-82), i.e. abs
    // y 20-56): that geometry is already proven safe within the round bezel
    // on shipped screens, so this reuses it exactly rather than guessing at
    // new numbers.
    lv_obj_t *title_cont = lv_obj_create(scr_notifications);
    lv_obj_set_size(title_cont, 180, 36);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_clear_flag(title_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(title_cont, LV_ALIGN_CENTER, 0, -82);
    // Whole bar is the back target, not just the two labels' tight glyph
    // bounds — same fix as screen_timeedit.cpp's header.
    lv_obj_add_flag(title_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title_cont, notif_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(title_cont, 8);

    lv_obj_t *back_arrow = lv_label_create(title_cont);
    lv_obj_set_style_text_font(back_arrow, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(back_arrow, color_gold, 0);
    lv_label_set_text(back_arrow, "<");

    lv_obj_t *title = lv_label_create(title_cont);
    lv_obj_set_style_text_font(title, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(title, color_ivory, 0);
    lv_label_set_text(title, "الإشعارات");

    // Scrollable list — 5 full-size touch-target rows don't fit statically
    // within the round bezel's safe area (verified against the same envelope
    // screen_timeedit.cpp's buttons use, abs y ~56-201: only ~140px tall once
    // narrow enough not to get corner-clipped), so the container is shorter
    // than the content and scrolls, same as any watch app with a list this
    // size. Width 190 keeps it clear of the bezel corners across that height.
    lv_obj_t *list = lv_obj_create(scr_notifications);
    lv_obj_set_size(list, 190, 140);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < NOTIF_ROWS; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, LV_PCT(100), NOTIF_ROW_H);
        // Not style_card — its 12px pad_all leaves almost no room in a 34px
        // row. Same look (surface/border/radius), lighter padding instead.
        lv_obj_set_style_bg_color(row, color_surface, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_border_color(row, color_border, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_set_style_pad_ver(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row, notif_row_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *label = lv_label_create(row);
        lv_obj_set_style_text_font(label, &font_alexandria_12, 0);
        lv_obj_set_style_text_color(label, color_cream, 0);
        lv_label_set_text(label, reminders[i].label);

        lv_obj_t *time_lbl = lv_label_create(row);
        lv_obj_set_style_text_font(time_lbl, &font_alexandria_12, 0);
        lv_obj_set_style_text_color(time_lbl, color_cream_dim, 0);
        lv_label_set_text_fmt(time_lbl, "%02d:%02d", reminders[i].hour, reminders[i].minute);
        notif_time_labels[i] = time_lbl;

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 38, 20);
        // The switch's own 38x20 box is too small a target for a finger next
        // to the row's much bigger clickable area — mis-taps were landing on
        // the row (opens the time editor) instead of the switch. Extend the
        // *hit-test* area past the visual box without changing layout or how
        // it looks. Capped at 8px: rows are only 40px apart center-to-center
        // (34px row + 6px gap) with a 20px-tall switch, so anything much
        // bigger would make adjacent rows' switches overlap and risk toggling
        // the wrong reminder instead — worse than the mis-tap we're fixing.
        lv_obj_set_ext_click_area(sw, 8);
        lv_obj_set_style_bg_color(sw, color_teal,
            (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
        if (reminders[i].enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, notif_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
        notif_switches[i] = sw;
    }
}
