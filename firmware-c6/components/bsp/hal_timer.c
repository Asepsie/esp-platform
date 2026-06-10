/**
 * @file hal_timer.c
 * @brief Timing HAL — ESP32-C6 target implementation.
 *
 * Monotonic time from the high-resolution esp_timer; delays yield to the
 * FreeRTOS scheduler. This is the only HAL timing source application code uses.
 */
#include "hal_timer.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t hal_timer_get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint32_t hal_timer_get_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

void hal_timer_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
