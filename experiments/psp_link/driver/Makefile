TARGET = psp_link
OBJS = main.o page_file.o accessory.o exports.o
USE_KERNEL_LIBS = 1
INCDIR = ../../../common
VPATH = ../../../common
CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu11 -fno-builtin-printf
ASFLAGS = $(CFLAGS)
LIBS = -lpspusbbus_driver -lpspusb_driver
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
LIBS += -lpspge_driver
