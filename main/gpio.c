#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "gpio.h"

#define BLINK_GPIO 11
#define GPIO_BUTTON_1 8
#define GPIO_BUTTON_2 10

static const char *TAG = "gpio";
static bool s_button_state[2] = {false}; 
static gpio_callback_t s_callback[2] = {NULL};

void GPIO_configure_io(void)
{
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void GPIO_toggle_led(void)
{
    static uint8_t s_led_state = 0;

    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);

    s_led_state ^= 1;
}

/**
 * @brief Task zur Überwachung des Buttons und Auslösung der Label-Aktualisierung.
 */
void GPIO_button_monitoring_task(void *pvParameter) {
    ESP_LOGI(TAG, "Button Monitoring Task gestartet");

    gpio_config_t io_conf;
    // Interrupt für fallende Flanke deaktivieren (wir pollen)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // Bitmaske des Pins
    io_conf.pin_bit_mask = (1ULL << GPIO_BUTTON_1) | (1ULL << GPIO_BUTTON_2);
    // Als Input setzen
    io_conf.mode = GPIO_MODE_INPUT;
    // Pull-up aktivieren
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    bool last_button_state[2] = {true}; 
    TickType_t last_press_time[2] = {0};
    const TickType_t debounce_delay = pdMS_TO_TICKS(50); // 50ms Entprellzeit

    while (1) {
        bool current_button_state[2] = {gpio_get_level(GPIO_BUTTON_1), gpio_get_level(GPIO_BUTTON_2)};

        // Prüfen auf fallende Flanke (Button gedrückt)
        if (last_button_state[0] == true && current_button_state[0] == false) {
            // Entprellen
            if ((xTaskGetTickCount() - last_press_time[0]) > debounce_delay) {
                last_press_time[0] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d gedrückt!", GPIO_BUTTON_1);
                s_button_state[0] = true;
                if(s_callback[0] != NULL) {
                    s_callback[0]();
                }
            }
        }
        // Prüfen auf steigende Flanke (Button losgelassen)
        else if (last_button_state[0] == false && current_button_state[0] == true) {
            if ((xTaskGetTickCount() - last_press_time[0]) > debounce_delay) {
                last_press_time[0] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d losgelassen!", GPIO_BUTTON_1);
                s_button_state[0] = false;
            }
        }

        // Prüfen auf fallende Flanke (Button gedrückt)
        if (last_button_state[1] == true && current_button_state[1] == false) {
            // Entprellen
            if ((xTaskGetTickCount() - last_press_time[1]) > debounce_delay) {
                last_press_time[1] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d gedrückt!", GPIO_BUTTON_2);
                s_button_state[1] = true;
                if(s_callback[1] != NULL) {
                    s_callback[1]();
                }
            }
        }
        // Prüfen auf steigende Flanke (Button losgelassen)
        else if (last_button_state[1] == false && current_button_state[1] == true) {
            if ((xTaskGetTickCount() - last_press_time[1]) > debounce_delay) {
                last_press_time[1] = xTaskGetTickCount();
                ESP_LOGI(TAG, "Button an GPIO %d losgelassen!", GPIO_BUTTON_2);
                s_button_state[1] = false;
            }
        }

        last_button_state[0] = current_button_state[0];
        last_button_state[1] = current_button_state[1];
        vTaskDelay(pdMS_TO_TICKS(20)); // Alle 20ms prüfen
    }
}

bool GPIO_get_button_set(void)
{
    return s_button_state[1];
}

bool GPIO_get_button_enter(void)
{
    return s_button_state[0];
}

void GPIO_register_callback_button_set(gpio_callback_t callback)
{
    s_callback[1] = callback;
}

void GPIO_register_callback_button_enter(gpio_callback_t callback)
{
    s_callback[0] = callback;
}