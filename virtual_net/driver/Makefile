TARGET = psp_vnet
OBJS = main.o interface.o mbuf.o lab_gateway.o exports.o
VPATH = ../../common
INCDIR = ../../common
USE_KERNEL_LIBS = 1
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -std=gnu11 -fno-builtin-printf
ASFLAGS = $(CFLAGS)
LIBS = -lpspsystemctrl_kernel -lpspsysmem_kernel_660
ifeq ($(USB_LAB),1)
OBJS += usb_transport.o accessory.o ssl_trace.o
CFLAGS += -DVNET_USB_LAB
LIBS += -lpspusbbus_driver -lpspusb_driver -lpsprtc_driver
endif
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
LIBS += -lpspge_driver

main.o: ifhandle_661_guard.h interface.h ../control.h
interface.o: interface.h

mbuf.o: mbuf.h

main.o: mbuf.h ../../common/lab_gateway.h
lab_gateway.o: ../../common/lab_gateway.h

main.o: frame_queue.h log_queue.h ../../common/packet_trace.h

main.o usb_transport.o: usb_transport.h

usb_transport.o: ../../common/native_wifi_scan.h ../../common/native_wifi_profile.h ../../common/packet_link.h ../../common/link_protocol.h

usb_transport.o: ../../common/link_diagnostics.h

main.o ssl_trace.o: ../../common/ssl_trace.h
ssl_trace.o: ../../common/ssl_trace.c ../../common/packet_trace.h
