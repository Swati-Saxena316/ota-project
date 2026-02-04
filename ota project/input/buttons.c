#include "buttons.h"
#include "config/pin_config.h"
#include "ui/buzzer.h"
#include "ota/ota_manager.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdio.h>

#define DEBOUNCE_MS     50
#define LONG_PRESS_MS   5000

static bool timer_running = false;
static int64_t press_start_time = 0;

static bool read_button(gpio_num_t pin) {
    static int64_t last_change_time[40] = {0};
    static bool last_state[40] = {false};

    bool current = (gpio_get_level(pin) == 0); // active LOW

    if (current != last_state[pin]) {
        last_state[pin] = current;
        last_change_time[pin] = esp_timer_get_time();
    }

    if ((esp_timer_get_time() - last_change_time[pin]) >
        (DEBOUNCE_MS * 1000)) {
        return current;
    }

    return false;
}

void buttons_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&cfg);
}

void buttons_process(void) {
    bool btn1 = read_button(BTN1_PIN);
    bool btn2 = read_button(BTN2_PIN);

    if (btn1 && btn2) {
        if (!timer_running) {
            press_start_time = esp_timer_get_time();
            timer_running = true;
        }

        if ((esp_timer_get_time() - press_start_time) >=
            (LONG_PRESS_MS * 1000)) {

            printf("[INPUT] OTA Trigger Detected\n");

            buzzer_beep(2);
            ota_set_state(OTA_STATE_CHECKING_WIFI);

            timer_running = false;
        }
    } else {
        timer_running = false;
    }
}
