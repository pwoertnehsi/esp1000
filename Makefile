.PHONY: psp esp package test probe virtual browser usb-virtual wifi-plugin
psp:
	$(MAKE) -C experiments/psp_link package

# Activate ESP-IDF first: source /opt/esp-idf/export.sh
esp:
	IDF_COMPONENT_MANAGER=0 idf.py -C esp_link build

package: psp esp
	python3 tools/package_esp.py

test:
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/browser_paths_test.c browser_local/driver/path_patch.c -o /tmp/psp-browser-path-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-browser-path-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Icommon tests/http_response_test.c common/lab_gateway.c -o /tmp/psp-http-response-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-http-response-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Icommon tests/packet_link_test.c common/lab_gateway.c -o /tmp/psp-packet-link-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-packet-link-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Icommon tests/link_test.c -o /tmp/psp-esp-link-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-esp-link-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Icommon tests/file_receiver_test.c -o /tmp/psp-esp-file-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-esp-file-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/vnet_interface_test.c virtual_net/driver/interface.c -o /tmp/psp-vnet-interface-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-vnet-interface-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Icommon tests/lab_gateway_test.c common/lab_gateway.c -o /tmp/psp-lab-gateway-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-lab-gateway-test
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/vnet_mbuf_test.c virtual_net/driver/mbuf.c -o /tmp/psp-vnet-mbuf-test
	ASAN_OPTIONS=detect_leaks=0 /tmp/psp-vnet-mbuf-test
	python3 -m unittest discover -s tests -p 'test_*.py'

# Separate memory-stick diagnostic; does not overwrite the USB demo package.
probe:
	$(MAKE) -C experiments/net_probe package

# Separate native interface registration experiment, requiring 6.61 ARK-5.
virtual:
	$(MAKE) -C virtual_net clean
	$(MAKE) -C virtual_net package

# Clean avoids accidentally reusing objects compiled for the other backend.
usb-virtual:
	$(MAKE) -C virtual_net USB_LAB=1 clean
	$(MAKE) -C virtual_net clean
	$(MAKE) -C virtual_net USB_LAB=1 all
	mkdir -p build/virtual-net-usb/PSP/GAME/psp-vnet-usb
	cp virtual_net/app/EBOOT.PBP virtual_net/driver/psp_vnet.prx virtual_net/USB_README.txt build/virtual-net-usb/PSP/GAME/psp-vnet-usb/
	cd build/virtual-net-usb && python3 -m zipfile -c ../virtual-net-usb-test.zip PSP

# Personal firmware stays in ignored staging; ZIP contains only our code/page.
browser:
	$(MAKE) -C browser_local package

wifi-plugin:
	$(MAKE) -C virtual_net/driver USB_LAB=1
	$(MAKE) -C wifi_plugin/native_usb
	$(MAKE) -C wifi_plugin/native_session
	$(MAKE) -C wifi_plugin
	$(MAKE) -C wifi_plugin/menu_bridge
	$(MAKE) -C browser_local/app
	python3 tools/package_wifi_plugin.py

# Current supported build. Python prepares upstream sources and packages binaries;
# all compiler invocations and dependency order belong to Make.
.DEFAULT_GOAL := production
PSP_COMPONENTS := virtual_net/driver wifi_plugin/native_usb wifi_plugin/native_session wifi_plugin wifi_plugin/menu_bridge
AEMU_COMPONENTS := atpro memab pspnet_adhoc_auth pspnet_adhoc pspnet_adhocctl pspnet_adhoc_matching pspnet_shims
.PHONY: production debug current-psp install esp-production esp-debug
production:
	$(MAKE) current-psp PROFILE=production DIAGNOSTICS=0 OUTPUT="$(or $(OUTPUT),build/production)"
debug:
	$(MAKE) current-psp PROFILE=debug DIAGNOSTICS=1 OUTPUT="$(or $(OUTPUT),build/debug)"
current-psp:
	@test ! -e "$(OUTPUT)" || { echo 'Use a fresh OUTPUT directory; existing builds are preserved.'; exit 1; }
	mkdir -p "$(OUTPUT)"
	@set -e; for component in $(PSP_COMPONENTS); do \
		$(MAKE) -C $$component clean USB_LAB=1 DIAGNOSTICS=$(DIAGNOSTICS); \
		$(MAKE) -C $$component USB_LAB=1 DIAGNOSTICS=$(DIAGNOSTICS); \
	done
	python3 tools/prepare_aemu.py --output "$(OUTPUT)/aemu-build" $(if $(filter production,$(PROFILE)),--production,)
	@set -e; for component in $(AEMU_COMPONENTS); do \
		$(MAKE) -C "$(OUTPUT)/aemu-build/source/$$component" CONFIG_660=1 RELEASE=1; \
	done
	python3 tools/package_psp.py --profile $(PROFILE) --output "$(OUTPUT)" $(if $(SERVER),--server "$(SERVER)",)
install:
	@test -n "$(PSP)" || { echo 'Set PSP to the mounted memory-stick root.'; exit 1; }
	python3 tools/install_psp.py --build "$(or $(BUILD),build/production/psp)" --psp "$(PSP)"
esp-production:
	mkdir -p esp_link/build-production
	IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-production -D SDKCONFIG=build-production/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.production' build
esp-debug:
	mkdir -p esp_link/build-debug
	IDF_COMPONENT_MANAGER=0 idf.py -C esp_link -B esp_link/build-debug -D SDKCONFIG=build-debug/sdkconfig -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build

# pspdecrypt is only needed to extract Sony's browser files.
PSPDECRYPT := third_party/pspdecrypt/pspdecrypt
PSPDECRYPT_CC ?= clang
PSPDECRYPT_CXX ?= clang++
FW_MANIFEST := build/firmware/661-extracted/browser-assets.json
.PHONY: pspdecrypt browser-firmware

$(PSPDECRYPT):
	git submodule update --init --recursive third_party/pspdecrypt
	$(MAKE) -C third_party/pspdecrypt CC=$(PSPDECRYPT_CC) CXX=$(PSPDECRYPT_CXX)

pspdecrypt: $(PSPDECRYPT)

$(FW_MANIFEST): $(PSPDECRYPT)
	python3 tools/prepare_browser_firmware.py --download

browser-firmware: $(FW_MANIFEST)
