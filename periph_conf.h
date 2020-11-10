/* Board peripheral configuration for the application,
 * this will get preprocessed first than the real board.h
 * file, so we "include next" it to make it available and
 * override any settings we want */

#ifdef BOARD_ESP32_WROVER_KIT

#define I2C0_SPEED I2C_SPEED_LOW

#endif /* BOARD_ESP32_WROVER_KIT */

#include_next "periph_conf.h"
