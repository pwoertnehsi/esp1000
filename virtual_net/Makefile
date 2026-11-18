.PHONY: all package clean
all:
	$(MAKE) -C driver
	$(MAKE) -C app
package: all
	mkdir -p ../artifacts/virtual-net/PSP/GAME/psp-vnet
	cp app/EBOOT.PBP driver/psp_vnet.prx README.txt ../artifacts/virtual-net/PSP/GAME/psp-vnet/
	cd ../artifacts/virtual-net && python3 -m zipfile -c ../virtual-net-test.zip PSP
clean:
	$(MAKE) -C driver clean
	$(MAKE) -C app clean
