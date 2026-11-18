#pragma once

#include "esp_hidd.h"

// HID Consumer Usage IDs (subset of the codes available in the USB HID Usage Tables spec)
typedef enum
{
    HID_CONSUMER_POWER = 48,
    HID_CONSUMER_RESET = 49,
    HID_CONSUMER_SLEEP = 50,

    HID_CONSUMER_MENU = 64,
    HID_CONSUMER_SELECTION = 128,
    HID_CONSUMER_ASSIGN_SEL = 129,
    HID_CONSUMER_MODE_STEP = 130,
    HID_CONSUMER_RECALL_LAST = 131,
    HID_CONSUMER_QUIT = 148,
    HID_CONSUMER_HELP = 149,
    HID_CONSUMER_CHANNEL_UP = 156,
    HID_CONSUMER_CHANNEL_DOWN = 157,

    HID_CONSUMER_PLAY = 176,
    HID_CONSUMER_PAUSE = 177,
    HID_CONSUMER_RECORD = 178,
    HID_CONSUMER_FAST_FORWARD = 179,
    HID_CONSUMER_REWIND = 180,
    HID_CONSUMER_SCAN_NEXT_TRK = 181,
    HID_CONSUMER_SCAN_PREV_TRK = 182,
    HID_CONSUMER_STOP = 183,
    HID_CONSUMER_EJECT = 184,
    HID_CONSUMER_RANDOM_PLAY = 185,
    HID_CONSUMER_SELECT_DISC = 186,
    HID_CONSUMER_ENTER_DISC = 187,
    HID_CONSUMER_REPEAT = 188,
    HID_CONSUMER_STOP_EJECT = 204,
    HID_CONSUMER_PLAY_PAUSE = 205,
    HID_CONSUMER_PLAY_SKIP = 206,

    HID_CONSUMER_VOLUME = 224,
    HID_CONSUMER_BALANCE = 225,
    HID_CONSUMER_MUTE = 226,
    HID_CONSUMER_BASS = 227,
    HID_CONSUMER_VOLUME_UP = 233,
    HID_CONSUMER_VOLUME_DOWN = 234
} hid_key_t;

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

typedef struct
{
    hid_key_t key;
    bool pressed;
} hid_cmd_t;

void ble_setup(gpio_num_t status_led_pin);

uint8_t ble_send(hid_cmd_t cmd);