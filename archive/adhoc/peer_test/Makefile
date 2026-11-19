TARGET = esp-adhoc-peer
OBJS = main.o
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ESP1000 Ad Hoc Peer Test
PSP_FW_VERSION = 661
CFLAGS = -O2 -G0 -Wall -Wextra -Werror
LIBS = -lpspnet -lpspnet_adhoc -lpspnet_adhocctl -lpsputility
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
