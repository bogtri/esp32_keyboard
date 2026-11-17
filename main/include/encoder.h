#pragma once

#include "freertos/queue.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

typedef struct
{
    uint8_t device_id;
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    uint32_t low_limit;
    uint32_t hight_limit;
    QueueHandle_t queue;
} encoder_config_t;

typedef struct
{
    uint8_t device_id;
    pcnt_unit_handle_t pcnt;
    QueueHandle_t queue;
} encoder_handle_t;

void encoder_setup(const encoder_config_t *config, encoder_handle_t *handle);