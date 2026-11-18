TARGET = wifi_native_session
OBJS = main.o adapter.o dns.o lookup.o native_config.o native_config_abi.o native_http.o lab_gateway.o exports.o
INCDIR = ../../common
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -fno-pic -DVNET_USB_LAB -DNATIVE_HTTP_LOCAL_ONLY
PSP_FW_VERSION = 661
LIBS = -lpspkubridge -lpspsystemctrl_user -Wl,--start-group -lpthreadglue -lpthread -lcglue -lc -Wl,--end-group -lpspnet -lpspnet_inet -lpspnet_resolver -lpsputility
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
adapter.o dns.o lookup.o: %.o: ../../browser_local/app/%.c
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@
native_config.o native_config_abi.o native_http.o: %.o: ../../virtual_net/app/%.c
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@
lab_gateway.o: ../../common/lab_gateway.c
	$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@
main.o: ../native_control.h
