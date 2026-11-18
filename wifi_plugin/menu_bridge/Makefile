TARGET = wifi_menu
OBJS = tls_offload.o main.o native_adapter.o trampoline.o imports.o exports.o
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -fno-pic
ASFLAGS = $(CFLAGS)
LIBS = -lpspkubridge -lpsputility -lpspusb -lpspnet_inet
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
main.o: ../menu_control.h

main.o native_adapter.o: ../native_control.h native_adapter.h ../../common/native_wifi_profile.h ../../common/native_sony_profile.h

tls_offload.o: ../tls_control.h ../../common/tls_offload.h
