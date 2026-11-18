TARGET = psp-link
OBJS = main.o
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP ESP Link
PSP_FW_VERSION = 661
CFLAGS = -O2 -G0 -Wall -Wextra
LIBS = -lpspkubridge -lpsputility
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
