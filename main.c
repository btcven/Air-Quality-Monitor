/*   Copyright 2020 Locha Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <string.h>

#include "lvgl/lvgl.h"
#include "lvgl_riot.h"

#include "screen_dev.h"

#include "ili9341.h"
#include "ili9341_params.h"
#include "disp_dev.h"
#include "ili9341_disp_dev.h"

#include "fmt.h"
#include "saul_reg.h"
#include "xtimer.h"

#define REFR_TIME          (5000)

static void airquality_create_humidity(lv_obj_t *parent);
static void airquality_update_humidity(void);

static void airquality_create_temperature(lv_obj_t *parent);
static void airquality_update_temperature(void);

static void airquality_create_pressure(lv_obj_t *parent);
static void airquality_update_pressure(void);

static void airquality_create_particulate(lv_obj_t *parent);
static void airquality_update_particulate(void);

static ili9341_t s_disp_dev;
static screen_dev_t s_screen;

static lv_obj_t *win;
static lv_task_t *refr_task;

static lv_obj_t *humidity_lmeter;
static lv_obj_t *humidity_label;
static saul_reg_t *humidity_sensor;

static lv_obj_t *temperature_gauge;
static lv_obj_t *temperature_label;
static saul_reg_t *temperature_sensor;

static lv_obj_t *pressure_label;
static saul_reg_t *pressure_sensor;

static lv_obj_t *particulate_label;
static saul_reg_t *particulate_sensor;

/* C doesn't have a sane pow function that uses only integers, and us, the poor
 * men have to write our own, less tested alternative :-).
 *
 * TODO: test it!
 */
static inline uint32_t _poor_man_pow10(unsigned p) {
    uint32_t result = 1;
    uint32_t x = 10;

    while (p) {
        if (p & 0x1) {
            result *= x;
        }

        x *= x;
        p >>= 1;
    }

    return result;
}

static void airquality_create_humidity(lv_obj_t *parent)
{
    humidity_sensor = saul_reg_find_type(SAUL_SENSE_HUM);
    if (humidity_sensor == NULL) {
        LOG_ERROR("Humidity sensor not found\n");
        return;
    }
    LOG_INFO("Humidity sensor found\n");

    lv_coord_t grid_w_meter = lv_page_get_width_grid(parent, 3, 1);
    lv_coord_t meter_h = lv_page_get_height_fit(parent);
    lv_coord_t meter_size = LV_MATH_MIN(grid_w_meter, meter_h);

    humidity_lmeter = lv_linemeter_create(parent, NULL);
    lv_obj_set_size(humidity_lmeter, meter_size, meter_size);
    lv_linemeter_set_range(humidity_lmeter, 0, 100);
    lv_linemeter_set_value(humidity_lmeter, 0);
    lv_linemeter_set_scale(humidity_lmeter, 240, 10);

    humidity_label = lv_label_create(humidity_lmeter, NULL);
    lv_label_set_text(humidity_label, "Humidity");
    lv_obj_align(humidity_label, humidity_lmeter, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
}

static void airquality_update_humidity(void)
{
    if (humidity_sensor == NULL) {
        return;
    }

    phydat_t humidity;
    if (saul_reg_read(humidity_sensor, &humidity) < 0) {
        LOG_ERROR("Couldn't read humidity\n");
        return;
    }

    /* update humidity linemeter */
    int32_t hum_val = 0;
    if (humidity.scale > 0) {
        hum_val = humidity.val[0] * _poor_man_pow10((unsigned)humidity.scale);
    }
    else if (humidity.scale < 0) {
        hum_val = humidity.val[0] / _poor_man_pow10((unsigned)-humidity.scale);
    }
    if (hum_val > 100) {
        hum_val = 100;
    }
    lv_linemeter_set_value(humidity_lmeter, hum_val);

    char str_hum[16];
    size_t len = fmt_s16_dfp(str_hum, humidity.val[0], humidity.scale);
    str_hum[len] = '\0';

    lv_label_set_text_fmt(humidity_label, "%s %s", str_hum, phydat_unit_to_str(humidity.unit));
    lv_obj_realign(humidity_label);
}

static void airquality_create_temperature(lv_obj_t *parent)
{
    temperature_sensor = saul_reg_find_type(SAUL_SENSE_TEMP);
    if (temperature_sensor == NULL) {
        LOG_ERROR("Temperature sensor not found\n");
        return;
    }
    LOG_INFO("Temperature sensor found\n");

    lv_coord_t grid_w_meter = lv_page_get_width_grid(parent, 3, 1);
    lv_coord_t meter_h = lv_page_get_height_fit(parent);
    lv_coord_t meter_size = LV_MATH_MIN(grid_w_meter, meter_h);

    temperature_gauge = lv_gauge_create(parent, NULL);
    lv_gauge_set_scale(temperature_gauge, 240, 31, 0);
    lv_gauge_set_critical_value(temperature_gauge, 70);
    lv_gauge_set_range(temperature_gauge, 0, 100);
    lv_gauge_set_value(temperature_gauge, 0, 0);
    lv_obj_set_size(temperature_gauge, meter_size, meter_size);
    lv_obj_set_style_local_value_str(temperature_gauge, LV_GAUGE_PART_MAIN,
                                     LV_STATE_DEFAULT, "Temperature");

    temperature_label = lv_label_create(temperature_gauge, NULL);
    lv_label_set_text(temperature_label, "Temperature");
    lv_obj_align(temperature_label, temperature_gauge, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
}

static void airquality_update_temperature(void)
{
    if (temperature_sensor == NULL) {
        return;
    }

    phydat_t temperature;
    if (saul_reg_read(temperature_sensor, &temperature) < 0) {
        LOG_ERROR("Couldn't read temperature\n");
        return;
    }

    int temp_val = 0;
    if (temperature.scale > 0) {
        temp_val = temperature.val[0] * _poor_man_pow10((unsigned)temperature.scale);
    }
    else if (temperature.scale < 0) {
        temp_val = temperature.val[0] / _poor_man_pow10((unsigned)-temperature.scale);
    }
    if (temp_val > 100) {
        temp_val = 100;
    }
    lv_gauge_set_value(temperature_gauge, 0, temp_val);

    char str_temp[32];
    size_t len = fmt_s16_dfp(str_temp, temperature.val[0], temperature.scale);
    str_temp[len] = '\0';

    lv_label_set_text_fmt(temperature_label, "%s %s", str_temp,
                          phydat_unit_to_str(temperature.unit));
    lv_obj_realign(temperature_label);
}

static void airquality_create_pressure(lv_obj_t *parent)
{
    pressure_sensor = saul_reg_find_type(SAUL_SENSE_PRESS);
    if (pressure_sensor == NULL) {
        LOG_ERROR("Pressure sensor not found\n");
        return;
    }
    LOG_INFO("Pressure sensor found\n");

    pressure_label = lv_label_create(parent, NULL);
    lv_label_set_text(pressure_label, "Pressure");
}

static void airquality_update_pressure(void)
{
    if (pressure_sensor == NULL) {
        return;
    }

    phydat_t pressure;
    if (saul_reg_read(pressure_sensor, &pressure) < 0) {
        LOG_ERROR("Couldn't read pressure\n");
        return;
    }

    char str_press[16];
    size_t len = fmt_s16_dfp(str_press, pressure.val[0], pressure.scale);
    str_press[len] = '\0';

    lv_label_set_text_fmt(pressure_label, "%s %s", str_press, phydat_unit_to_str(pressure.unit));
}

static void airquality_create_particulate(lv_obj_t *parent)
{
    particulate_sensor = saul_reg_find_type(SAUL_SENSE_PM);
    if (particulate_sensor == NULL) {
        LOG_ERROR("Particulate Matter sensor not found\n");
        return;
    }
    LOG_INFO("Particulate Matter sensor found\n");

    particulate_label = lv_label_create(parent, NULL);
    lv_label_set_text(particulate_label, "Particulate Matter");
}

static void airquality_update_particulate(void)
{
    if (particulate_sensor == NULL) {
        return;
    }

    phydat_t particulate;
    if (saul_reg_read(particulate_sensor, &particulate) < 0) {
        LOG_ERROR("Couldn't read particulate sensor\n");
        return;
    }
    printf("density=%d scale=%d\n", (int)particulate.val[0], (int)particulate.scale);

    char str_pm[16];
    size_t len = fmt_s16_dfp(str_pm, particulate.val[0], particulate.scale);
    str_pm[len] = '\0';

    lv_label_set_text_fmt(particulate_label, "%s %s", str_pm, phydat_unit_to_str(particulate.unit));
}

static void airquality_task(lv_task_t *param)
{
    (void)param;

    airquality_update_humidity();
    airquality_update_temperature();
    airquality_update_pressure();
    airquality_update_particulate();

    /* Force a wakeup of lvgl when each task is called: this ensures an activity
       is triggered and wakes up lvgl during the next LVGL_INACTIVITY_PERIOD ms */
    lvgl_wakeup();
}

void airquality_create(void)
{
    /* Air Quality window, responsive */
    win = lv_win_create(lv_disp_get_scr_act(NULL), NULL);
    lv_win_set_title(win, "Air Quality Monitor");
    lv_win_set_layout(win, LV_LAYOUT_PRETTY_TOP);

    airquality_create_humidity(win);
    airquality_create_temperature(win);
    airquality_create_pressure(win);
    airquality_create_particulate(win);

    /* Refresh UI */
    airquality_task(NULL);
    refr_task = lv_task_create(airquality_task, REFR_TIME, LV_TASK_PRIO_LOW, NULL);
}

int main(void)
{
    /* Configure the generic display driver interface */
    s_screen.display = (disp_dev_t *)&s_disp_dev;
    s_screen.display->driver = &ili9341_disp_dev_driver;

    LOG_INFO("Turning on the LCD backlight\n");
    /* Enable backlight */
    disp_dev_backlight_on();

    LOG_INFO("Initializing ILI9341 display\n");
    /* Initialize the concrete display driver */
    ili9341_init(&s_disp_dev, &ili9341_params[0]);

    LOG_INFO("Initializing LittleVGL library\n");
    /* Initialize lvgl with the generic display and touch drivers */
    lvgl_init(&s_screen);

    /* Create the system monitor widget */
    airquality_create();

    return 0;
}
