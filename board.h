/* Board configuration for the application,
 * this will get preprocessed first than the real board.h
 * file, so we "include next" it to make it available and
 * override any settings we want */

#ifdef BOARD_ESP32_WROVER_KIT

#define I2C_SPEED               I2C_SPEED_LOW

#define BMX280_PARAM_I2C_ADDR   (0x76)
#define GP2Y10XX_PARAM_ILED_PIN GPIO_PIN(0, 22)
#define GP2Y10XX_PARAM_VREF     (3300)
#define GP2Y10XX_PARAM_ADC_RES  (ADC_RES_12BIT)

#endif /* BOARD_ESP32_WROVER_KIT */

#include_next "board.h"
