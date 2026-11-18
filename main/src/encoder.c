#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "encoder.h"
#include "input.h"

static bool pcnt_on_reach_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    encoder_handle_t *handle = (encoder_handle_t *)user_ctx;

    int current_value = edata->watch_point_value;
    pcnt_unit_clear_count(unit);

    input_event_t event; 
    event.device_id = handle->device_id;
    event.type = current_value == 1 ? IN_EVENT_ENCODER_INCREASED : IN_EVENT_ENCODER_DECREASED;

    xQueueSendFromISR(handle->queue, &event, &high_task_wakeup);

    return (high_task_wakeup == pdTRUE);
}

void encoder_setup(const encoder_config_t *config, encoder_handle_t *handle)
{
    pcnt_unit_config_t unit_config = {
        .high_limit = config->hight_limit,
        .low_limit = config->low_limit,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = config->pin_a,
        .level_gpio_num = config->pin_b,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    //TODO should be configurable
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, -1));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, 1));

    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));

    //TODO callbacks should be configurable
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_on_reach_cb,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, handle));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));

    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));

    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    handle->device_id = config->device_id;
    handle->pcnt = pcnt_unit;
    handle->queue = config->queue;
}