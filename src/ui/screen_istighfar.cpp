/*
 * screen_istighfar.c  —  Istighfar counter
 *
 * Layout (240×240 circle):
 *
 *      [ استغفار ]            ← title, gold, small
 *
 *   [ أَستَغفِرُ الله ]        ← arabic phrase, ivory, large
 *
 *        [  0  ]             ← counter, ivory, 48px
 *
 *    من 100 · اضغط للعد      ← hint, cream-dim, small
 *
 *  Gold progress arc (r≈110px) sweeps as count increases.
 *  Tap anywhere on screen to increment.
 *  At 100 the counter resets to 0.
 */

#include "screens.h"
#include "styles.h"
#include <Preferences.h>
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);
LV_FONT_DECLARE(font_reem_kufi_48);

#define ISTIGHFAR_TARGET 100

extern Preferences prefs;
uint32_t istighfarCount = 0;
uint32_t totalIstighfar = 0;

lv_obj_t *istighfar_arc         = NULL;
lv_obj_t *istighfar_count_label = NULL;

void update_istighfar_display(void)
{
    if (!istighfar_count_label) return;
    lv_label_set_text_fmt(istighfar_count_label, "%lu", (unsigned long)istighfarCount);
    lv_arc_set_value(istighfar_arc, (int)istighfarCount);
}

static void istighfar_tap_cb(lv_event_t *e)
{
    if (lv_screen_active() != scr_istighfar) return;

    istighfarCount++;
    totalIstighfar++;
    prefs.putUInt("totalisteghfar", totalIstighfar);
    if (istighfarCount > ISTIGHFAR_TARGET) istighfarCount = 0;

    update_istighfar_display();
    prefs.putUInt("isteghfar", istighfarCount);
}

void create_screen_istighfar(void)
{
    scr_istighfar = lv_obj_create(NULL);
    make_screen_base(scr_istighfar);

    lv_obj_add_flag(scr_istighfar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_istighfar, istighfar_tap_cb, LV_EVENT_CLICKED, NULL);

    // ── Progress arc ─────────────────────────────────────────
    istighfar_arc = lv_arc_create(scr_istighfar);
    lv_obj_set_size(istighfar_arc, 220, 220);
    lv_obj_center(istighfar_arc);
    lv_arc_set_bg_angles(istighfar_arc, 0, 360);
    lv_arc_set_range(istighfar_arc, 0, ISTIGHFAR_TARGET);
    lv_arc_set_value(istighfar_arc, 0);
    lv_arc_set_mode(istighfar_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(istighfar_arc, 270);
    lv_obj_remove_style(istighfar_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(istighfar_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(istighfar_arc, lv_color_hex(0x0a1f10), LV_PART_MAIN);
    lv_obj_set_style_arc_width(istighfar_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(istighfar_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(istighfar_arc, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_arc_color(istighfar_arc, color_teal, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(istighfar_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(istighfar_arc, true, LV_PART_INDICATOR);

    // ── Title  "استغفار" ─────────────────────────────────────
    lv_obj_t *title = lv_label_create(scr_istighfar);
    lv_obj_set_style_text_font(title, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(title, color_teal, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(title,
        "\330\247\330\263\330\252\330\272\331\201\330\247\330\261");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -68);

    // ── Arabic phrase  "أَستَغفِرُ الله" ──────────────────────
    lv_obj_t *phrase = lv_label_create(scr_istighfar);
    lv_obj_set_style_text_font(phrase, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(phrase, color_ivory, 0);
    lv_obj_set_style_text_align(phrase, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(phrase, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(phrase, 180);
    lv_label_set_text(phrase,
        "\330\243\330\263\330\252\330\272\331\201\330\261 "
        "\330\247\331\204\331\204\331\207");
    lv_obj_align(phrase, LV_ALIGN_CENTER, 0, -30);

    // ── Counter digit ─────────────────────────────────────────
    istighfar_count_label = lv_label_create(scr_istighfar);
    lv_obj_set_style_text_font(istighfar_count_label, &font_reem_kufi_48, 0);
    lv_obj_set_style_text_color(istighfar_count_label, color_ivory, 0);
    lv_obj_set_style_text_align(istighfar_count_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(istighfar_count_label, "0");
    lv_obj_align(istighfar_count_label, LV_ALIGN_CENTER, 0, 28);

    // ── Hint  "من 100 · اضغط للعد" ───────────────────────────
    lv_obj_t *hint = lv_label_create(scr_istighfar);
    lv_obj_set_style_text_font(hint, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(hint, color_cream_dim, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint,
        "\330\247\330\266\330\272\330\267 \331\204\331\204\330\271\330\257"
        " \331\205\331\206 100 ");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 75);
}
