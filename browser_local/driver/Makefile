TARGET = browser_paths
OBJS = main.o path_patch.o code_patch.o asset_device.o exports.o
USE_KERNEL_LIBS = 1
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -fno-pic -fno-builtin-printf
ASFLAGS = $(CFLAGS)
LIBS = -lpspsystemctrl_kernel -lpspsysmem_kernel_660
PSP_FW_VERSION = 661
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
main.o: ../control.h browser_guards.h path_patch.h code_patch.h apctl_guards.h browser_gate_guards.h
path_patch.o: path_patch.h

asset_device.o: asset_device.h
