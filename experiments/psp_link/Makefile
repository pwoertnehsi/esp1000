.PHONY: all clean package
all:
	$(MAKE) -C driver
	$(MAKE) -C app

package: all
	mkdir -p ../../build/psp/PSP/GAME/psp-link
	cp app/EBOOT.PBP driver/psp_link.prx ../../build/psp/PSP/GAME/psp-link/
	python3 -m zipfile -c ../../build/psp-link-test.zip ../../build/psp/PSP

clean:
	$(MAKE) -C driver clean
	$(MAKE) -C app clean
