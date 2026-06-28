#include "screens.h"
#include <Arduino.h>

// ── Ring state ───────────────────────────────────────────────
//   [0] Home  ←→  [1] Istighfar  ←→  [2] Tasbeeh  ←→  [0]
lv_obj_t *ring_screens[RING_LEN];
int current_ring_idx = 0;

void navigate_to_ring(int idx) {
#if NAV_DEBUG
    Serial.printf("[NAV] navigate_to_ring(%d)  current=%d -> %d\n",
        idx, current_ring_idx, idx);
#endif
    current_ring_idx = idx;
    lv_screen_load(ring_screens[idx]);
}

void navigate_ring(int delta) {
    int next = (current_ring_idx + delta + RING_LEN) % RING_LEN;
    lv_scr_load_anim_t anim = (delta > 0)
        ? LV_SCR_LOAD_ANIM_MOVE_LEFT
        : LV_SCR_LOAD_ANIM_MOVE_RIGHT;
#if NAV_DEBUG
    Serial.printf("[NAV] navigate_ring(%+d)  idx %d -> %d  anim=%s\n",
        delta, current_ring_idx, next,
        (delta > 0) ? "MOVE_LEFT" : "MOVE_RIGHT");
#endif
    lv_screen_load_anim(ring_screens[next], anim, 200, 0, false);
    current_ring_idx = next;
}

void gesture_ring_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
#if NAV_DEBUG
    Serial.printf("[GESTURE] gesture_ring_cb  dir=0x%02x (%s%s%s%s)  active_scr=%p\n",
        dir,
        (dir == LV_DIR_LEFT)  ? "LEFT "  : "",
        (dir == LV_DIR_RIGHT) ? "RIGHT " : "",
        (dir == LV_DIR_TOP)   ? "TOP "   : "",
        (dir == LV_DIR_BOTTOM)? "BOTTOM ": "",
        lv_screen_active());
#endif
    if (dir == LV_DIR_LEFT)  navigate_ring(+1);
    if (dir == LV_DIR_RIGHT) navigate_ring(-1);
    if (dir == LV_DIR_BOTTOM && current_ring_idx == 0) push_modal(scr_settings);
}

// ── Modal state ──────────────────────────────────────────────
static lv_obj_t *modal_return_screen = NULL;

void push_modal(lv_obj_t *dest) {
    modal_return_screen = lv_screen_active();
#if NAV_DEBUG
    Serial.printf("[MODAL] push_modal  return=%p  dest=%p\n",
        modal_return_screen, dest);
#endif
    lv_screen_load_anim(dest, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
}

void pop_modal(void) {
#if NAV_DEBUG
    Serial.printf("[MODAL] pop_modal  return=%p  active=%p\n",
        modal_return_screen, lv_screen_active());
#endif
    if (!modal_return_screen) {
        lv_screen_load(scr_home);
        return;
    }
    lv_screen_load_anim(modal_return_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
    modal_return_screen = NULL;
}

void gesture_modal_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
#if NAV_DEBUG
    Serial.printf("[GESTURE] gesture_modal_cb  dir=0x%02x (%s%s%s%s)\n",
        dir,
        (dir == LV_DIR_LEFT)  ? "LEFT "  : "",
        (dir == LV_DIR_RIGHT) ? "RIGHT " : "",
        (dir == LV_DIR_TOP)   ? "TOP "   : "",
        (dir == LV_DIR_BOTTOM)? "BOTTOM ": "");
#endif
    if (dir == LV_DIR_BOTTOM) pop_modal();
    if (dir == LV_DIR_LEFT)   pop_modal();
}

void modal_bg_tap_cb(lv_event_t *e) {
#if NAV_DEBUG
    Serial.println("[CLICK] modal background -> pop_modal");
#endif
    pop_modal();
}
