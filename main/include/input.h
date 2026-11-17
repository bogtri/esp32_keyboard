#pragma once

typedef enum
{
    IN_EVENT_BUTTON_PRESSED,
    IN_EVENT_BUTTON_RELEASED,
    IN_EVENT_ENCODER_INCREASED,
    IN_EVENT_ENCODER_DECREASED
} input_event_type;

typedef struct
{
    input_event_type type;
    uint8_t device_id;
} input_event_t;