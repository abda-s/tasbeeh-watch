// Simulator LVGL config: the firmware's exact config, plus a bigger heap.
// The firmware's 64KB LVGL pool is sized for 32-bit ESP32 pointers; on a
// 64-bit desktop the same objects are larger, so the pool needs headroom.
#pragma once
#include "../src/config/lv_conf.h"
#undef  LV_MEM_SIZE
#define LV_MEM_SIZE (512 * 1024U)
