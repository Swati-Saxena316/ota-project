#include "buzzer.h"
#include "config/pin_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BEEP_ON_MS   200
#define BEEP_OFF_MS  200

void buzzer_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUZZER_PIN,
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&cfg);
    gpio_set_level(BUZZER_PIN, 0);
}

void buzzer_beep(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(BEEP_ON_MS));
        gpio_set_level(BUZZER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(BEEP_OFF_MS));
    }
}
