TARGET = browser-test
OBJS = native_dialog.o main.o adapter.o dns.o lookup.o browser_state.o native_config.o native_config_abi.o native_http.o native_file.o lab_gateway.o
INCDIR = ../../common
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ESP Browser Test
PSP_FW_VERSION = 661
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -fno-pic -DVNET_USB_LAB
LIBS = -lpspkubridge -lpspsystemctrl_user -Wl,--start-group -lpthreadglue -lpthread -lcglue -lc -Wl,--end-group -lpspgu -lpspge -lpspdisplay -lpspdebug -lpspctrl -lpsputility -lpspnet -lpspnet_inet -lpspnet_resolver -lpspnet_apctl -lpsphttp -lpspssl
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
main.o: ../control.h assets.h

adapter.o: adapter.h ../../virtual_net/control.h
main.o: adapter.h dns.h ../../virtual_net/control.h

native_config.o native_config_abi.o native_http.o native_file.o: %.o: ../../virtual_net/app/%.c
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@
lab_gateway.o: ../../common/lab_gateway.c
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@

lookup.o main.o: lookup.h

browser_state.o main.o: browser_state.h

main.o native_http.o: ../../virtual_net/app/native_http.h

native_dialog.o main.o: native_dialog.h ../../wifi_plugin/menu_control.h
