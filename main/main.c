/* ESP32-C3-TFT-CUBE

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_lvgl_port.h"

#include "bsp/esp-bsp.h"

static const char *TAG = "cube";



/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;
static int8_t s_arc_value = 0;
static uint32_t s_arc_count = 0;

void lv_screen_timer_arc(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

  lv_arc_set_value(user, s_arc_value);

  s_arc_value = 50.0 * cos((2.0*3.184*s_arc_count)/200.0);
  s_arc_value += 50;
  s_arc_count++;
}

void lv_screen_timer_led(lv_timer_t* timer)
{
  /*Use the user_data*/
  lv_obj_t* user = lv_timer_get_user_data(timer);

  lv_led_toggle(user);
}

void lv_screen_1(void)
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);

    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(lv_screen_active());
    lv_arc_set_mode(arc, LV_ARC_MODE_SYMMETRICAL);
    lv_obj_set_size(arc, 110, 110);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_set_style_arc_color(arc,lv_palette_main(LV_PALETTE_INDIGO), LV_PART_INDICATOR);
    lv_arc_set_value(arc, s_arc_value);
    lv_obj_center(arc);

    /*Create a LED*/
    lv_obj_t * led = lv_led_create(lv_screen_active());
    lv_obj_align(led, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_brightness(led, 150);
    lv_led_set_color(led, lv_palette_main(LV_PALETTE_INDIGO));
    lv_led_on(led);

    lv_timer_t * timer_led = lv_timer_create(lv_screen_timer_led, 1000, led);
    lv_timer_t * timer_arc = lv_timer_create(lv_screen_timer_arc, 100, arc);
}

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_lvgl_display(void)
{
    bsp_display_lock(0);
    lv_screen_1();
    bsp_display_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    // spi_device_handle_t spi_handle = NULL;

    // const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // esp_err_t err = lvgl_port_init(&lvgl_cfg);

    /* Configure the peripheral according to the LED type */
    configure_led();



    // lvgl_spi_driver_init(TFT_SPI_HOST,
    //     DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
    //     SPI_BUS_MAX_TRANSFER_SZ, 1,
    //     DISP_SPI_IO2, DISP_SPI_IO3);


    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "display start failed!");
        abort();
    }
    app_lvgl_display();


    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
