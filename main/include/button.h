#pragma once

typedef struct
{
    gpio_num_t pin;
    uint8_t device_id;
    uint8_t state;
    TimerHandle_t debounce_timer;
    QueueHandle_t event_queue;
} button_handle_t;

void button_setup(gpio_num_t pin, uint8_t id, QueueHandle_t event_queue, button_handle_t *handle);