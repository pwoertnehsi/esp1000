.PHONY: all clean package
all:
	$(MAKE) -C driver
	$(MAKE) -C app

package: all
	mkdir -p ../artifacts/psp/PSP/GAME/psp-link
	cp app/EBOOT.PBP driver/psp_link.prx ../artifacts/psp/PSP/GAME/psp-link/
	python3 -m zipfile -c ../artifacts/psp-link-test.zip ../artifacts/psp/PSP

clean:
	$(MAKE) -C driver clean
	$(MAKE) -C app clean
