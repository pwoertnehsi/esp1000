TARGET = wifi_vsh
OBJS = main.o menu_runtime.o native_runtime.o browser_runtime.o browser_paths.o browser_assets.o exports.o
USE_KERNEL_LIBS = 1
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -fno-pic
LIBS = -lpspsystemctrl_kernel -lpspsysmem_kernel_660 -lpspinit_kernel_660 -lpspctrl_driver -lpspusb_driver -lpspge_driver -lpspsdk
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
main.o: policy.h menu_control.h menu_runtime.h
menu_runtime.o: menu_runtime.h menu_guards.h menu_patch.h menu_control.h

package: all
	$(MAKE) -C ../virtual_net/driver USB_LAB=1
	$(MAKE) -C native_usb
	$(MAKE) -C native_session
	$(MAKE) -C menu_bridge
	$(MAKE) -C ../browser_local/driver
	$(MAKE) -C ../browser_local/app
	python3 ../tools/package_wifi_plugin.py


native_runtime.o: native_control.h native_runtime.h ../common/native_wifi_scan.h
main.o: native_runtime.h

native_runtime.o: native_import_guards.h

browser_paths.o: ../browser_local/driver/path_patch.c
	$(CC) $(CFLAGS) -c $< -o $@
browser_assets.o: ../browser_local/driver/asset_device.c
	$(CC) $(CFLAGS) -c $< -o $@
browser_runtime.o: browser_runtime.h ../browser_local/driver/browser_guards.h
native_runtime.o: browser_import_guards.h browser_import_hooks.h
