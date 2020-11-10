#include_next "board.h"
#undef ILI9341_PARAM_RGB
#undef ILI9341_PARAM_INVERTED

/* Board configuration for the application,
 * this will get preprocessed first than the real board.h
 * file, so we "include next" it to make it available and
 * override any settings we want */
#ifdef BOARD_ESP32_WROVER_KIT

/* BME/BMP 280 sensor settings */
#define BMX280_PARAM_I2C_ADDR   (0x76)

/* Optical dust sensor settings */
#define GP2Y10XX_PARAM_ILED_PIN GPIO_PIN(0, 22)
#define GP2Y10XX_PARAM_VREF     (3300)
#define GP2Y10XX_PARAM_ADC_RES  (ADC_RES_12BIT)

/* LCD settings */
#define ILI9341_PARAM_RGB       0
#define ILI9341_PARAM_INVERTED  0

#endif /* BOARD_ESP32_WROVER_KIT */
