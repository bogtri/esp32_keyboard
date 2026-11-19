#include "stdio.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "input.h"
#include "button.h"

const static TickType_t debounce_time = pdMS_TO_TICKS(30);

const static char *TAG = "button.c";

static void debounce_timer_callback(TimerHandle_t xTimer)
{
    button_handle_t *btn = (button_handle_t *)pvTimerGetTimerID(xTimer);
    uint8_t level = gpio_get_level(btn->pin);
    if (level != btn->state)
    {
        input_event_type type = level == 0 ? IN_EVENT_BUTTON_PRESSED : IN_EVENT_BUTTON_RELEASED;
        input_event_t event = {.device_id = btn->device_id, .type = type};
        btn->state = level;
        if (xQueueSend(btn->event_queue, &event, 0) != pdPASS)
        {
            ESP_LOGI(TAG, "Cannot send event to input_queue");
        }
    }
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    button_handle_t *btn = (button_handle_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTimerResetFromISR(btn->debounce_timer, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_setup(gpio_num_t pin, uint8_t id, QueueHandle_t event_queue, button_handle_t *handle)
{
    handle->device_id = id;
    handle->pin = pin;
    handle->event_queue = event_queue;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = 1ULL << pin};
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    handle->state = gpio_get_level(pin);

    char timer_name[32];
    snprintf(timer_name, sizeof(timer_name), "debounce_timer%u", id);

    handle->debounce_timer = xTimerCreate(timer_name, debounce_time, pdFALSE, (void *)handle, debounce_timer_callback);
    assert(handle->debounce_timer);

    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, gpio_isr_handler, (void *)handle));
}