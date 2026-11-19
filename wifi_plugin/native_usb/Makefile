TARGET = wifi_native_usb
OBJS = main.o usb_transport.o accessory.o exports.o
USE_KERNEL_LIBS = 1
INCDIR = ../../common
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -std=gnu11
ASFLAGS = $(CFLAGS)
LIBS = -lpspusbbus_driver -lpspusb_driver -lpspsysmem_kernel_660
PSP_FW_VERSION = 661
DIAGNOSTICS ?= 1
CFLAGS += -DESP1000_DIAGNOSTICS=$(DIAGNOSTICS)
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
usb_transport.o: ../../virtual_net/driver/usb_transport.c ../../virtual_net/driver/usb_transport.h ../../common/native_wifi_scan.h
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@
accessory.o: ../../virtual_net/driver/accessory.S
	$(CC) $(ASFLAGS) -I$(PSPSDK)/include -c $< -o $@
main.o: ../native_control.h

main.o usb_transport.o: ../../common/native_wifi_profile.h ../../common/link_protocol.h

usb_transport.o: ../../common/link_diagnostics.h
