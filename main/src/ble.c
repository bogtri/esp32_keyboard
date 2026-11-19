#include "stdint.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "esp_hidd.h"

#include "ble.h"
#include "esp_hid_gap.h"

#define HID_RPT_ID_CC_IN 3  // Consumer Control input report ID
#define HID_CC_IN_RPT_LEN 2 // Consumer Control input report Len

#define HID_CC_RPT_MUTE 1
#define HID_CC_RPT_POWER 2
#define HID_CC_RPT_LAST 3
#define HID_CC_RPT_ASSIGN_SEL 4
#define HID_CC_RPT_PLAY 5
#define HID_CC_RPT_PAUSE 6
#define HID_CC_RPT_RECORD 7
#define HID_CC_RPT_FAST_FWD 8
#define HID_CC_RPT_REWIND 9
#define HID_CC_RPT_SCAN_NEXT_TRK 10
#define HID_CC_RPT_SCAN_PREV_TRK 11
#define HID_CC_RPT_STOP 12

#define HID_CC_RPT_CHANNEL_UP 0x10
#define HID_CC_RPT_CHANNEL_DOWN 0x30
#define HID_CC_RPT_VOLUME_UP 0x40
#define HID_CC_RPT_VOLUME_DOWN 0x80

// HID Consumer Control report bitmasks
#define HID_CC_RPT_NUMERIC_BITS 0xF0
#define HID_CC_RPT_CHANNEL_BITS 0xCF
#define HID_CC_RPT_VOLUME_BITS 0x3F
#define HID_CC_RPT_BUTTON_BITS 0xF0
#define HID_CC_RPT_SELECTION_BITS 0xCF

// Macros for the HID Consumer Control 2-byte report
#define HID_CC_RPT_SET_NUMERIC(s, x)   \
    (s)[0] &= HID_CC_RPT_NUMERIC_BITS; \
    (s)[0] = (x)

#define HID_CC_RPT_SET_CHANNEL(s, x)   \
    (s)[0] &= HID_CC_RPT_CHANNEL_BITS; \
    (s)[0] |= ((x) & 0x03) << 4

#define HID_CC_RPT_SET_VOLUME_UP(s)   \
    (s)[0] &= HID_CC_RPT_VOLUME_BITS; \
    (s)[0] |= HID_CC_RPT_VOLUME_UP

#define HID_CC_RPT_SET_VOLUME_DOWN(s) \
    (s)[0] &= HID_CC_RPT_VOLUME_BITS; \
    (s)[0] |= HID_CC_RPT_VOLUME_DOWN

#define HID_CC_RPT_SET_BUTTON(s, x)   \
    (s)[1] &= HID_CC_RPT_BUTTON_BITS; \
    (s)[1] |= (x)

#define HID_CC_RPT_SET_SELECTION(s, x)   \
    (s)[1] &= HID_CC_RPT_SELECTION_BITS; \
    (s)[1] |= ((x) & 0x03) << 4

typedef enum
{
    BLE_INITIALIZING,
    BLE_ADVERTISING,
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_SLEEP
} ble_status_t;

typedef struct
{
    gpio_num_t status_led_pin;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
    ble_status_t status;
} ble_handle_t;

const static char *TAG = "BLE";

static uint8_t mediaReportMap[] = {
    0x05,
    0x0C, //   Usage Page (Consumer)
    0x09,
    0x01, //   Usage (Consumer Control)
    0xA1,
    0x01, //   Collection (Application)
    0x85,
    0x03, //   Report ID (3)
    0x09,
    0x02, //   Usage (Numeric Key Pad)
    0xA1,
    0x02, //   Collection (Logical)
    0x05,
    0x09, //   Usage Page (Button)
    0x19,
    0x01, //   Usage Minimum (0x01)
    0x29,
    0x0A, //   Usage Maximum (0x0A)
    0x15,
    0x01, //   Logical Minimum (1)
    0x25,
    0x0A, //   Logical Maximum (10)
    0x75,
    0x04, //   Report Size (4)
    0x95,
    0x01, //   Report Count (1)
    0x81,
    0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, //   End Collection
    0x05,
    0x0C, //   Usage Page (Consumer)
    0x09,
    0x86, //   Usage (Channel)
    0x15,
    0xFF, //   Logical Minimum (-1)
    0x25,
    0x01, //   Logical Maximum (1)
    0x75,
    0x02, //   Report Size (2)
    0x95,
    0x01, //   Report Count (1)
    0x81,
    0x46, //   Input (Data,Var,Rel,No Wrap,Linear,Preferred State,Null State)
    0x09,
    0xE9, //   Usage (Volume Increment)
    0x09,
    0xEA, //   Usage (Volume Decrement)
    0x15,
    0x00, //   Logical Minimum (0)
    0x75,
    0x01, //   Report Size (1)
    0x95,
    0x02, //   Report Count (2)
    0x81,
    0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09,
    0xE2, //   Usage (Mute)
    0x09,
    0x30, //   Usage (Power)
    0x09,
    0x83, //   Usage (Recall Last)
    0x09,
    0x81, //   Usage (Assign Selection)
    0x09,
    0xB0, //   Usage (Play)
    0x09,
    0xB1, //   Usage (Pause)
    0x09,
    0xB2, //   Usage (Record)
    0x09,
    0xB3, //   Usage (Fast Forward)
    0x09,
    0xB4, //   Usage (Rewind)
    0x09,
    0xB5, //   Usage (Scan Next Track)
    0x09,
    0xB6, //   Usage (Scan Previous Track)
    0x09,
    0xB7, //   Usage (Stop)
    0x15,
    0x01, //   Logical Minimum (1)
    0x25,
    0x0C, //   Logical Maximum (12)
    0x75,
    0x04, //   Report Size (4)
    0x95,
    0x01, //   Report Count (1)
    0x81,
    0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09,
    0x80, //   Usage (Selection)
    0xA1,
    0x02, //   Collection (Logical)
    0x05,
    0x09, //   Usage Page (Button)
    0x19,
    0x01, //   Usage Minimum (0x01)
    0x29,
    0x03, //   Usage Maximum (0x03)
    0x15,
    0x01, //   Logical Minimum (1)
    0x25,
    0x03, //   Logical Maximum (3)
    0x75,
    0x02, //   Report Size (2)
    0x81,
    0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, //   End Collection
    0x81,
    0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0, //   End Collection
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = mediaReportMap,
     .len = sizeof(mediaReportMap)}};

const static esp_hid_device_config_t ble_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "ESP32 Keyboard",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = ble_report_maps,
    .report_maps_len = 1};

static ble_handle_t handle = {0};

void esp_hidd_send_consumer_value(uint8_t key_cmd, bool key_pressed)
{
    uint8_t buffer[HID_CC_IN_RPT_LEN] = {0, 0};
    if (key_pressed)
    {
        switch (key_cmd)
        {
        case HID_CONSUMER_CHANNEL_UP:
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_UP);
            break;

        case HID_CONSUMER_CHANNEL_DOWN:
            HID_CC_RPT_SET_CHANNEL(buffer, HID_CC_RPT_CHANNEL_DOWN);
            break;

        case HID_CONSUMER_VOLUME_UP:
            HID_CC_RPT_SET_VOLUME_UP(buffer);
            break;

        case HID_CONSUMER_VOLUME_DOWN:
            HID_CC_RPT_SET_VOLUME_DOWN(buffer);
            break;

        case HID_CONSUMER_MUTE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_MUTE);
            break;

        case HID_CONSUMER_POWER:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_POWER);
            break;

        case HID_CONSUMER_RECALL_LAST:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_LAST);
            break;

        case HID_CONSUMER_ASSIGN_SEL:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_ASSIGN_SEL);
            break;

        case HID_CONSUMER_PLAY:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PLAY);
            break;

        case HID_CONSUMER_PAUSE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PAUSE);
            break;

        case HID_CONSUMER_RECORD:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_RECORD);
            break;

        case HID_CONSUMER_FAST_FORWARD:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_FAST_FWD);
            break;

        case HID_CONSUMER_REWIND:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_REWIND);
            break;

        case HID_CONSUMER_SCAN_NEXT_TRK:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_NEXT_TRK);
            break;

        case HID_CONSUMER_SCAN_PREV_TRK:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_PREV_TRK);
            break;

        case HID_CONSUMER_STOP:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_STOP);
            break;

        default:
            break;
        }
    }
    esp_hidd_dev_input_set(handle.hid_dev, 0, HID_RPT_ID_CC_IN, buffer, HID_CC_IN_RPT_LEN);
    return;
}

static void ble_status_led_on(void)
{
    gpio_set_level(handle.status_led_pin, 1);
}

static void ble_status_led_off(void)
{
    gpio_set_level(handle.status_led_pin, 0);
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_EVENT";

    switch (event)
    {
    case ESP_HIDD_START_EVENT:
    {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT:
    {
        ESP_LOGI(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT:
    {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT:
    {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {
            // exit suspend
            handle.status = BLE_CONNECTED;
            ble_status_led_on();
        }
        else
        {
            // suspend
            handle.status = BLE_SLEEP;
        }
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT:
    {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT:
    {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT:
    {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        handle.status = BLE_DISCONNECTED;
        ble_status_led_off();
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_STOP_EVENT:
    {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

void ble_hid_device_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

void ble_hid_task_start_up(void)
{
    handle.status = BLE_CONNECTED;
    ble_status_led_on();
}

void ble_setup(gpio_num_t status_led_pin)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_DEV_MODE);
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK(ret);

    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, ble_hid_config.device_name);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &handle.hid_dev));

    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    handle.status_led_pin = status_led_pin;
    handle.status = BLE_INITIALIZING;

    /* Starting nimble task after gatts is initialized*/
    ret = esp_nimble_enable(ble_hid_device_host_task);
    if (ret)
    {
        ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
    }
}

uint8_t ble_send(hid_cmd_t cmd)
{
    if (handle.status == BLE_CONNECTED)
    {
        esp_hidd_send_consumer_value(cmd.key, cmd.pressed);
        return 0;
    }
    else
    {
        ESP_LOGW(TAG, "BLE is not CONNECTED, skipping command: key = %u, pressed = %u", cmd.key, cmd.pressed);
    }
    return 1;
}