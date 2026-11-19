#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "soc/rtc.h"

#include "ble.h"
#include "input.h"
#include "encoder.h"
#include "button.h"

const static char *TAG = "LOG";

const static TickType_t queue_send_timeout = pdMS_TO_TICKS(1000);

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
    .pin_b = GPIO_NUM_8};

static QueueHandle_t input_queue = NULL;
static QueueHandle_t ble_queue = NULL;

static button_handle_t button1;
static button_handle_t button2;

static encoder_handle_t encoder;

static void hid_enqueue(hid_key_t key, bool pressed)
{
    hid_cmd_t cmd = { .key = key, .pressed = pressed };
    if (xQueueSend(ble_queue, &cmd, queue_send_timeout) != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to send HID command to queue: key=%u, pressed=%u", key, pressed);
    }
}

static void input_task(void *arg)
{
    ESP_LOGI(TAG, "Input task started");
    input_event_t event;

    while (1)
    {
        if (xQueueReceive(input_queue, &event, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Received input event: device_id=%u, event_type=%u", event.device_id, event.type);

            switch (event.type)
            {
            case IN_EVENT_BUTTON_PRESSED:
                if (event.device_id == button1_id)
                {
                    hid_enqueue(HID_CONSUMER_STOP, true);
                }
                else if (event.device_id == button2_id)
                {
                    hid_enqueue(HID_CONSUMER_PLAY, true);
                }
                else
                {
                    ESP_LOGW(TAG, "Unknown device ID %u", event.device_id);
                }
                break;
            case IN_EVENT_BUTTON_RELEASED:
                if (event.device_id == button1_id)
                {
                    hid_enqueue(HID_CONSUMER_STOP, false);
                    
                }
                else if (event.device_id == button2_id)
                {
                    hid_enqueue(HID_CONSUMER_PLAY, false);
                }
                else
                {
                    ESP_LOGW(TAG, "Unknown device ID %u", event.device_id);
                }
                break;
            case IN_EVENT_ENCODER_INCREASED:
                hid_enqueue(HID_CONSUMER_VOLUME_UP, true);
                vTaskDelay(pdMS_TO_TICKS(100));
                hid_enqueue(HID_CONSUMER_VOLUME_UP, false);
                break;
            case IN_EVENT_ENCODER_DECREASED:
                hid_enqueue(HID_CONSUMER_VOLUME_DOWN, true);
                vTaskDelay(pdMS_TO_TICKS(100));
                hid_enqueue(HID_CONSUMER_VOLUME_DOWN, false);
                break;
            default:
                break;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Cannot consume input queue");
        }
    }
}

static void ble_task(void *arg)
{
    ESP_LOGI(TAG, "BLE task started");
    hid_cmd_t cmd;

    while (1)
    {
        if (xQueueReceive(ble_queue, &cmd, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Received HID report: key = %u, pressed = %u", cmd.key, cmd.pressed);
            ble_send(cmd);
        }
        else
        {
            ESP_LOGE(TAG, "Cannot consume BLE queue");
        }
    }
}

void app_main(void)
{
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_t pm_config = {
            .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
            .min_freq_mhz = rtc_clk_xtal_freq_get(),
            .light_sleep_enable = true
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

    ESP_ERROR_CHECK(gpio_reset_pin(led_pin));
    ESP_ERROR_CHECK(gpio_set_direction(led_pin, GPIO_MODE_OUTPUT));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    input_queue = xQueueCreate(10, sizeof(input_event_t));
    assert(input_queue);

    ble_queue = xQueueCreate(10, sizeof(hid_cmd_t));
    assert(ble_queue);

    button_setup(button1_pin, button1_id, input_queue, &button1);
    button_setup(button2_pin, button2_id, input_queue, &button2);

    encoder_config.queue = input_queue;
    encoder_setup(&encoder_config, &encoder);

    assert(xTaskCreatePinnedToCore(input_task, "input_task", 2048, NULL, 5, NULL, 1) == pdPASS);
    assert(xTaskCreatePinnedToCore(ble_task, "ble_task", 2048, NULL, 10, NULL, 1) == pdPASS);

    ble_setup(led_pin);
}