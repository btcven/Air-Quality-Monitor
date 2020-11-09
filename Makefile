APPLICATION = airquality
BOARD ?= esp32-wrover-kit
QUIET ?= 1

RIOTBASE = $(CURDIR)/../RIOT

USEPKG += lvgl
USEMODULE += lvgl_contrib
USEMODULE += ili9341

USEMODULE += bme280_i2c
USEMODULE += gp2y10xx
USEMODULE += saul_default

USEMODULE += fmt

LVGL_MEM_SIZE ?= 32*1024U
LVGL_TASK_THREAD_PRIO ?= THREAD_PRIORITY_MAIN+1

INCLUDES += -I$(APPDIR)

include $(RIOTBASE)/Makefile.include
