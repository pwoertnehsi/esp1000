.PHONY: all prepare package clean
prepare:
	python3 ../tools/prepare_browser_local.py
all: prepare
	$(MAKE) -C ../virtual_net/driver USB_LAB=1 clean
	$(MAKE) -C ../virtual_net/driver USB_LAB=1
	$(MAKE) -C driver
	$(MAKE) -C app
package: all
	python3 ../tools/prepare_browser_local.py
	python3 ../tools/package_browser_code.py
clean:
	$(MAKE) -C driver clean
	$(MAKE) -C app clean
