#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "input.h"
#include "encoder.h"
#include "button.h"

const static char *TAG = "LOG";

//const static TickType_t queue_send_timeout = pdMS_TO_TICKS(100);

const static gpio_num_t led_pin = GPIO_NUM_4;
const static gpio_num_t button1_pin = GPIO_NUM_5;
const static gpio_num_t button2_pin = GPIO_NUM_6;

const static uint8_t button1_id = 0;
const static uint8_t button2_id = 1;
const static uint8_t encoder_id = 2;

static encoder_config_t encoder_config = {
    .device_id = encoder_id,
    .hight_limit = 1,
    .low_limit = -1,
    .pin_a = GPIO_NUM_7,
    .pin_b = GPIO_NUM_8
};

static TaskHandle_t button_task_handle = NULL;

static QueueHandle_t input_queue = NULL;

static button_handle_t button1;
static button_handle_t button2;

static encoder_handle_t encoder;

static void input_task(void *arg)
{
    ESP_LOGI(TAG, "Input task started");
    input_event_t event;

    while (1)
    {
        if (xQueueReceive(input_queue, &event, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Got event: %u, %u", event.device_id, event.type);
        }
        else
        {
            ESP_LOGE(TAG, "Cannot consume input queue");
        }
    }
}

void app_main(void)
{
    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    gpio_install_isr_service(0);

    input_queue = xQueueCreate(10, sizeof(input_event_t));
    assert(input_queue);

    button_setup(button1_pin, button1_id, input_queue, &button1);
    button_setup(button2_pin, button2_id, input_queue, &button2);

    encoder_config.queue = input_queue;
    encoder_setup(&encoder_config, &encoder);

    assert(xTaskCreatePinnedToCore(input_task, "input_task", 2048, NULL, 10, &button_task_handle, 1) == pdPASS);
}