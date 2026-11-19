TARGET = net-probe
OBJS = main.o native_check.o
INCDIR = ../../common
VPATH = ../../common
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP Wi-Fi Compatibility Check
PSP_FW_VERSION = 661
CFLAGS = -O2 -G0 -Wall -Wextra -Werror
# Resolve newlib glue before SDK stubs to keep each import table contiguous.
LIBS = -lpspkubridge -lpspsystemctrl_user -Wl,--start-group -lpthreadglue -lpthread -lcglue -lc -Wl,--end-group -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpsputility -lpspnet -lpspnet_inet
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

.PHONY: package
package: all
	mkdir -p ../../build/net-probe/PSP/GAME/net-probe
	cp EBOOT.PBP README.txt ../../build/net-probe/PSP/GAME/net-probe/
	cd ../../build/net-probe && python3 -m zipfile -c ../net-probe.zip PSP
