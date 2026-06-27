/*
 * screen_tasbeeh.c  —  Tasbeeh counter (3 phrases × 33)
 *
 * Layout (240×240 circle):
 *
 *       [ تسبيح ]             ← title, teal, small
 *
 *   [ سُبحَانَ الله ]          ← current phrase, ivory
 *
 *        [  0  ]             ← counter, ivory, 48px
 *
 *         من 33              ← of-label, cream-dim
 *
 *      ■ ■ ■                 ← 3 progress dots (gold dim / teal active)
 *
 *  Teal progress arc sweeps 0→33 per phrase, resets on advance.
 *  Tap anywhere → increment; at 33 → advance to next phrase (wraps).
 */

#include "screens.h"
#include "styles.h"
#include <Preferences.h>
#include <Arduino.h>

#define TASBEEH_TARGET 33
#define TASBEEH_PHRASES 3

static const char *PHRASE_SUBHAN =
    "\330\263\331\220\330\250\330\255\330\247\331\216\331\206\331\216 \330\247\331\204\331\204\331\207";
static const char *PHRASE_HAMD =
    "\330\247\331\204\330\255\331\216\331\205\330\257\331\220 \331\204\331\204\331\207";
static const char *PHRASE_AKBAR =
    "\330\247\331\204\331\204\331\207\331\220 \330\243\331\216\331\203\330\250\331\216\330\261";

static const char *phrases[TASBEEH_PHRASES] = {
    PHRASE_SUBHAN,
    PHRASE_HAMD,
    PHRASE_AKBAR,
};

extern Preferences prefs;
uint32_t tasbeehCount = 0;
static int  tasbeeh_phrase_idx = 0;

lv_obj_t *tasbeeh_arc          = NULL;
lv_obj_t *tasbeeh_count_label  = NULL;
lv_obj_t *tasbeeh_phrase_label = NULL;
lv_obj_t *tasbeeh_title_label  = NULL;
lv_obj_t *tasbeeh_of_label     = NULL;
lv_obj_t *tasbeeh_dots[3]      = {NULL, NULL, NULL};

void update_tasbeeh_display(void)
{
    if (!tasbeeh_count_label) return;

    lv_label_set_text_fmt(tasbeeh_count_label, "%lu", (unsigned long)tasbeehCount);
    lv_arc_set_value(tasbeeh_arc, (int)tasbeehCount);
    lv_label_set_text(tasbeeh_phrase_label, phrases[tasbeeh_phrase_idx]);

    for (int i = 0; i < TASBEEH_PHRASES; i++) {
        if (tasbeeh_dots[i]) {
            lv_obj_set_style_bg_color(tasbeeh_dots[i],
                (i == tasbeeh_phrase_idx) ? color_teal : color_gold, 0);
            lv_obj_set_style_bg_opa(tasbeeh_dots[i],
                (i == tasbeeh_phrase_idx) ? LV_OPA_COVER : LV_OPA_30, 0);
        }
    }
}

static void tasbeeh_tap_cb(lv_event_t *e)
{
    if (lv_screen_active() != scr_tasbeeh) return;

    tasbeehCount++;
    if (tasbeehCount >= (uint32_t)TASBEEH_TARGET) {
        tasbeehCount = 0;
        tasbeeh_phrase_idx = (tasbeeh_phrase_idx + 1) % TASBEEH_PHRASES;
    }

    update_tasbeeh_display();
    prefs.putUInt("tasbeeh", tasbeehCount);
}

void create_screen_tasbeeh(void)
{
    scr_tasbeeh = lv_obj_create(NULL);
    make_screen_base(scr_tasbeeh);

    lv_obj_add_flag(scr_tasbeeh, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_tasbeeh, tasbeeh_tap_cb, LV_EVENT_CLICKED, NULL);

    // ── Progress arc (teal) ───────────────────────────────────
    tasbeeh_arc = lv_arc_create(scr_tasbeeh);
    lv_obj_set_size(tasbeeh_arc, 220, 220);
    lv_obj_center(tasbeeh_arc);
    lv_arc_set_bg_angles(tasbeeh_arc, 0, 360);
    lv_arc_set_range(tasbeeh_arc, 0, TASBEEH_TARGET);
    lv_arc_set_value(tasbeeh_arc, 0);
    lv_arc_set_mode(tasbeeh_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(tasbeeh_arc, 270);
    lv_obj_remove_style(tasbeeh_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(tasbeeh_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(tasbeeh_arc, lv_color_hex(0x1a2e22), LV_PART_MAIN);
    lv_obj_set_style_arc_width(tasbeeh_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(tasbeeh_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tasbeeh_arc, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_arc_color(tasbeeh_arc, color_teal, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(tasbeeh_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(tasbeeh_arc, true, LV_PART_INDICATOR);

    // ── Title  "تسبيح" ────────────────────────────────────────
    tasbeeh_title_label = lv_label_create(scr_tasbeeh);
    lv_obj_set_style_text_font(tasbeeh_title_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(tasbeeh_title_label, color_teal, 0);
    lv_obj_set_style_text_align(tasbeeh_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(tasbeeh_title_label,
        "\330\252\330\263\330\250\331\212\330\255");
    lv_obj_align(tasbeeh_title_label, LV_ALIGN_CENTER, 0, -68);

    // ── Phrase label ──────────────────────────────────────────
    tasbeeh_phrase_label = lv_label_create(scr_tasbeeh);
    lv_obj_set_style_text_font(tasbeeh_phrase_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(tasbeeh_phrase_label, color_ivory, 0);
    lv_obj_set_style_text_align(tasbeeh_phrase_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(tasbeeh_phrase_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tasbeeh_phrase_label, 180);
    lv_label_set_text(tasbeeh_phrase_label, PHRASE_SUBHAN);
    lv_obj_align(tasbeeh_phrase_label, LV_ALIGN_CENTER, 0, -28);

    // ── Counter ───────────────────────────────────────────────
    tasbeeh_count_label = lv_label_create(scr_tasbeeh);
    lv_obj_set_style_text_font(tasbeeh_count_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(tasbeeh_count_label, color_ivory, 0);
    lv_obj_set_style_text_align(tasbeeh_count_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(tasbeeh_count_label, "0");
    lv_obj_align(tasbeeh_count_label, LV_ALIGN_CENTER, 0, 28);

    // ── "من 33" ───────────────────────────────────────────────
    tasbeeh_of_label = lv_label_create(scr_tasbeeh);
    lv_obj_set_style_text_font(tasbeeh_of_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(tasbeeh_of_label, color_cream_dim, 0);
    lv_obj_set_style_text_align(tasbeeh_of_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(tasbeeh_of_label, "\331\205\331\206 33");
    lv_obj_align(tasbeeh_of_label, LV_ALIGN_CENTER, 0, 68);

    // ── Progress dots ──────────────────────────────────────────
// ── Progress dots ──────────────────────────────────────────
    static const int DOT_W = 20, DOT_H = 6, DOT_GAP = 10;
    int total_w = TASBEEH_PHRASES * DOT_W + (TASBEEH_PHRASES - 1) * DOT_GAP;
    int start_x = -(total_w / 2);

    for (int i = 0; i < TASBEEH_PHRASES; i++) {
        tasbeeh_dots[i] = lv_obj_create(scr_tasbeeh);
        lv_obj_set_size(tasbeeh_dots[i], DOT_W, DOT_H);
        lv_obj_set_style_radius(tasbeeh_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(tasbeeh_dots[i], color_gold, 0);
        lv_obj_set_style_bg_opa(tasbeeh_dots[i], LV_OPA_30, 0);
        lv_obj_set_style_border_width(tasbeeh_dots[i], 0, 0);
        
        // 1. Calculate the reversed position index
        int rtl_index = (TASBEEH_PHRASES - 1) - i; 

        // 2. Apply it to the alignment
        lv_obj_align(tasbeeh_dots[i], LV_ALIGN_CENTER,
            start_x + rtl_index * (DOT_W + DOT_GAP) + DOT_W / 2,
            88);
    }

    update_tasbeeh_display();
}
