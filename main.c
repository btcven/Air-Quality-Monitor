/*
 * Copyright (C) 2020 Locha Inc
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <string.h>

#ifdef CPU_ESP32
#include "board.h"
#include "adc_arch.h"
#include "gp2y10xx_params.h"
#endif

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

#include "thread.h"

#define REFR_TIME          (600)
#define LOCHA_COLOR        "F8931C"
#define RIOT_R_COLOR       "BC1A29"
#define RIOT_G_COLOR       "3FA687"

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

static lv_obj_t *pressure_gauge;
static lv_obj_t *pressure_label;
static saul_reg_t *pressure_sensor;

static lv_obj_t *particulate_gauge;
static lv_obj_t *particulate_label;
static saul_reg_t *particulate_sensor;
static phydat_t particulate;

static lv_style_t style_box;

#define PA_PER_MBAR (100U)

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

    humidity_lmeter = lv_linemeter_create(parent, NULL);
    lv_obj_set_size(humidity_lmeter, 80, 80);
    lv_obj_add_style(humidity_lmeter, LV_LINEMETER_PART_MAIN, &style_box);
    lv_linemeter_set_range(humidity_lmeter, 0, 100);
    lv_linemeter_set_value(humidity_lmeter, 0);
    lv_linemeter_set_scale(humidity_lmeter, 240, 11);

    humidity_label = lv_label_create(humidity_lmeter, NULL);
    lv_label_set_text(humidity_label, "Humidity");
    lv_label_set_recolor(humidity_label, true);
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

    lv_label_set_text_fmt(humidity_label,
                          "%s "LV_TXT_COLOR_CMD LOCHA_COLOR" %s"LV_TXT_COLOR_CMD,
                          str_hum, phydat_unit_to_str(humidity.unit));
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

    temperature_gauge = lv_gauge_create(parent, NULL);
    lv_gauge_set_scale(temperature_gauge, 240, 21, 0);
    lv_gauge_set_range(temperature_gauge, 0, 100);
    lv_gauge_set_critical_value(temperature_gauge, 70);
    lv_gauge_set_value(temperature_gauge, 0, 0);
    lv_obj_set_size(temperature_gauge, 80, 80);
    lv_obj_set_style_local_value_str(temperature_gauge, LV_GAUGE_PART_MAIN,
                                     LV_STATE_DEFAULT, "Temperature");
    lv_obj_add_style(temperature_gauge, LV_GAUGE_PART_MAIN, &style_box);

    temperature_label = lv_label_create(temperature_gauge, NULL);
    lv_label_set_text(temperature_label, "Temperature");
    lv_label_set_recolor(temperature_label, true);
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

    lv_label_set_text_fmt(temperature_label,
                          "%s "LV_TXT_COLOR_CMD LOCHA_COLOR" %s"LV_TXT_COLOR_CMD,
                          str_temp,
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

    pressure_gauge = lv_gauge_create(parent, NULL);
    lv_gauge_set_scale(pressure_gauge, 240, 21, 0);
    lv_gauge_set_range(pressure_gauge, 0, 1089);
    lv_gauge_set_critical_value(pressure_gauge, 1000);
    lv_gauge_set_value(pressure_gauge, 0, 0);
    lv_obj_set_size(pressure_gauge, 80, 80);
    lv_obj_set_style_local_value_str(pressure_gauge, LV_GAUGE_PART_MAIN,
                                     LV_STATE_DEFAULT, "Pressure");
    lv_obj_add_style(pressure_gauge, LV_GAUGE_PART_MAIN, &style_box);

    pressure_label = lv_label_create(pressure_gauge, NULL);
    lv_label_set_text(pressure_label, "Pressure");
    lv_label_set_recolor(pressure_label, true);
    lv_obj_align(pressure_label, pressure_gauge, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
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

    /* 1 hPa == 1 mbar */
    if (pressure.scale == 2 && pressure.unit == UNIT_PA) {
        lv_gauge_set_value(pressure_gauge, 0, pressure.val[0]);
        lv_label_set_text_fmt(pressure_label,
                              "%d "LV_TXT_COLOR_CMD LOCHA_COLOR" mbar"LV_TXT_COLOR_CMD,
                              pressure.val[0]);
    }
    else {

        char str_press[32];
        size_t len = fmt_s16_dfp(str_press, pressure.val[0], pressure.scale);
        str_press[len] = '\0';

        lv_label_set_text_fmt(pressure_label,
                              "%s "LV_TXT_COLOR_CMD LOCHA_COLOR"%s"LV_TXT_COLOR_CMD,
                              str_press,
                              phydat_unit_to_str(pressure.unit));
    }
    lv_obj_realign(pressure_label);

    if (pressure.unit == UNIT_PA) {
        int val = 0;
        if (pressure.scale > 0) {
            val = pressure.val[0] * _poor_man_pow10((unsigned)pressure.scale);
        }
        else if (pressure.scale < 0) {
            val = pressure.val[0] / _poor_man_pow10((unsigned)-pressure.scale);
        }
        if (val > (int)(1089 * _poor_man_pow10(2))) {
            val = 1089 * _poor_man_pow10(2);
        }
        /* convert to hPa/mbar */
        lv_gauge_set_value(pressure_gauge, 0, val / _poor_man_pow10(2));
    }
}

static void airquality_create_particulate(lv_obj_t *parent)
{
    particulate_sensor = saul_reg_find_type(SAUL_SENSE_PM);
    if (particulate_sensor == NULL) {
        LOG_ERROR("Particulate Matter sensor not found\n");
        return;
    }
    LOG_INFO("Particulate Matter sensor found\n");

    particulate_gauge = lv_gauge_create(parent, NULL);
    lv_gauge_set_scale(particulate_gauge, 240, 21, 0);
    lv_gauge_set_range(particulate_gauge, 0, 1100);
    lv_gauge_set_critical_value(particulate_gauge, 500);
    lv_gauge_set_value(particulate_gauge, 0, 0);
    lv_obj_set_size(particulate_gauge, 84, 84);
    lv_obj_set_style_local_value_str(particulate_gauge, LV_GAUGE_PART_MAIN,
                                     LV_STATE_DEFAULT, "Particulate Matter");
    lv_obj_add_style(particulate_gauge, LV_GAUGE_PART_MAIN, &style_box);

    particulate_label = lv_label_create(particulate_gauge, NULL);
    lv_label_set_text(particulate_label, "Particulate Matter");
    lv_label_set_recolor(particulate_label, true);
    lv_obj_align(particulate_label, particulate_gauge, LV_ALIGN_IN_BOTTOM_MID, 0,
                 -12);
}

static void airquality_update_particulate(void)
{
    if (particulate_sensor == NULL) {
        return;
    }

    if (particulate.scale == -6 && particulate.unit == UNIT_GPM3) {
        lv_label_set_text_fmt(particulate_label,
                              "%d "LV_TXT_COLOR_CMD LOCHA_COLOR" ug/m3"LV_TXT_COLOR_CMD,
                              particulate.val[0]);

        lv_gauge_set_value(particulate_gauge, 0, particulate.val[0]);
    }
    else {
        char str_pm[32];
        size_t len = fmt_s16_dfp(str_pm, particulate.val[0], particulate.scale);
        str_pm[len] = '\0';

        lv_label_set_text_fmt(particulate_label,
                              "%s "LV_TXT_COLOR_CMD LOCHA_COLOR" %s"LV_TXT_COLOR_CMD,
                              str_pm,
                              phydat_unit_to_str(particulate.unit));
    }
    lv_obj_realign(particulate_label);
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
    /* initialize box style */
    lv_style_init(&style_box);
    lv_style_set_value_align(&style_box, LV_STATE_DEFAULT, LV_ALIGN_OUT_TOP_LEFT);
    lv_style_set_value_ofs_y(&style_box, LV_STATE_DEFAULT, - LV_DPX(15));
    lv_style_set_margin_top(&style_box, LV_STATE_DEFAULT, LV_DPX(5));

    /* Air Quality window, responsive */
    win = lv_win_create(lv_disp_get_scr_act(NULL), NULL);
    lv_win_set_title(win, "Air Quality Monitor");
    lv_win_set_layout(win, LV_LAYOUT_PRETTY_TOP);

    airquality_create_humidity(win);
    airquality_create_temperature(win);
    airquality_create_pressure(win);
    airquality_create_particulate(win);

    lv_obj_t *about_label = lv_label_create(win, NULL);
    lv_label_set_recolor(about_label, true);
    lv_label_set_text(about_label,
                      "Brought to you by "
                      LV_TXT_COLOR_CMD LOCHA_COLOR" Locha"LV_TXT_COLOR_CMD"\n"
                      "Powered by "
                      LV_TXT_COLOR_CMD RIOT_R_COLOR" R"LV_TXT_COLOR_CMD
                      LV_TXT_COLOR_CMD RIOT_G_COLOR" iot"LV_TXT_COLOR_CMD
                      " and LVGL");

    /* Refresh UI */
    airquality_task(NULL);
    refr_task = lv_task_create(airquality_task, REFR_TIME, LV_TASK_PRIO_LOW, NULL);
}

static char _stack[THREAD_STACKSIZE_SMALL];

static void *_event_loop(void *args)
{
    (void)args;

    while (1) {
        if (saul_reg_read(particulate_sensor, &particulate) < 0) {
            LOG_ERROR("Couldn't read particulate sensor\n");
        }

        xtimer_msleep(200);
    }

    return NULL;
}

int main(void)
{
#ifdef CPU_ESP32
    LOG_INFO("Setting ADC line %d attenuation to 11 dB\n", gp2y10xx_params[0].aout);
    adc_set_attenuation(gp2y10xx_params[0].aout, ADC_ATTENUATION_11_DB);
#endif

    /* Configure the generic display driver interface */
    s_screen.display = (disp_dev_t *)&s_disp_dev;
    s_screen.display->driver = &ili9341_disp_dev_driver;

    /* Enable backlight */
    LOG_INFO("Turning on the LCD backlight\n");
    disp_dev_backlight_on();

    LOG_INFO("Initializing ILI9341 display\n");
    /* Initialize the concrete display driver */
    ili9341_init(&s_disp_dev, &ili9341_params[0]);

    LOG_INFO("Initializing LVGL library\n");
    /* Initialize lvgl with the generic display and touch drivers */
    lvgl_init(&s_screen);

    /* Dark theme */
#if LV_USE_THEME_MATERIAL
    LV_THEME_DEFAULT_INIT(lv_color_hex(0xf8931c),
                          lv_theme_get_color_secondary(),
                          LV_THEME_MATERIAL_FLAG_DARK,
                          lv_theme_get_font_small(),
                          lv_theme_get_font_normal(),
                          lv_theme_get_font_subtitle(),
                          lv_theme_get_font_title());
#endif

    /* Create the system monitor widget */
    airquality_create();

    /* Create Particulate Matter thread */
    thread_create(_stack, sizeof(_stack), THREAD_PRIORITY_MAIN - 1,
                  THREAD_CREATE_STACKTEST, _event_loop, NULL, "gp2y10xx");


    return 0;
}
