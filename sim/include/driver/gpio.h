#pragma once
#include <Arduino.h>
#include "esp_sleep.h"
// Real IDF exposes per-pin hold in deep sleep on the S3, so the firmware skips
// the global gpio_deep_sleep_hold_* calls behind this macro.
#define SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP 1
typedef enum { GPIO_INTR_DISABLE = 0, GPIO_INTR_LOW_LEVEL = 4, GPIO_INTR_HIGH_LEVEL = 5 } gpio_int_type_t;
esp_err_t gpio_wakeup_enable(gpio_num_t gpio_num, gpio_int_type_t intr_type);
esp_err_t gpio_hold_en(gpio_num_t gpio_num);
esp_err_t gpio_hold_dis(gpio_num_t gpio_num);
void gpio_deep_sleep_hold_en(void);
void gpio_deep_sleep_hold_dis(void);
