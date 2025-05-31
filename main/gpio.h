#pragma once 

extern void GPIO_configure_io(void);
extern void GPIO_toggle_led(void);
extern void GPIO_button_monitoring_task(void *pvParameter);
extern bool GPIO_get_button_set(void);
extern bool GPIO_get_button_enter(void);

// Callback function type definition
typedef void (*gpio_callback_t)(void);

// Function to register a callback
extern void GPIO_register_callback_button_set(gpio_callback_t callback);
extern void GPIO_register_callback_button_enter(gpio_callback_t callback);