TARGET = psp-vnet
OBJS = main.o native_config.o native_config_abi.o lab_client.o lab_gateway.o
VPATH = ../../common
INCDIR = ../../common
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Virtual Interface Test
ifeq ($(USB_LAB),1)
PSP_EBOOT_TITLE = PSP Native Internet Test
endif
PSP_FW_VERSION = 661
CFLAGS = -O2 -G0 -Wall -Wextra -Werror
ifeq ($(USB_LAB),1)
CFLAGS += -DVNET_USB_LAB
OBJS += native_http.o native_file.o
endif
# Resolve newlib glue before SDK import stubs.
LIBS = -lpspkubridge -lpspsystemctrl_user -Wl,--start-group -lpthreadglue -lpthread -lcglue -lc -Wl,--end-group -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpsputility -lpspnet -lpspnet_inet
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

main.o: ../control.h lab_client.h
native_config.o: native_config.h ../../common/lab_gateway.h
lab_client.o: lab_client.h native_config.h ../../common/lab_gateway.h
lab_gateway.o: ../../common/lab_gateway.h

native_config_abi.o: native_config.h
