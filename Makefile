.PHONY: psp esp package test probe virtual browser usb-virtual wifi-plugin
psp:
	$(MAKE) -C psp_link package

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
	$(MAKE) -C net_probe package

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

# Current native plugins, clean-built together; output must be a fresh directory.
.PHONY: production debug
production:
	python3 tools/build_psp.py --profile production --output $(or $(OUTPUT),build/production)
debug:
	python3 tools/build_psp.py --profile debug --output $(or $(OUTPUT),build/debug)
