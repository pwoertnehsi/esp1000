# Current plugin roles and configuration

## VSH modules

wifi_vsh.prx runs in kernel mode. It restores native XMB menu paths, performs
exact guarded firmware/import patches, registers the browser asset filesystem,
provides the accessory-power lease, and exports the controlled kernel services
used by the user bridge.

wifi_menu.prx runs in user mode. It supplies the APCTL scan/connection/handler
facade, reads saved profiles through Sony utility APIs, forwards native XMB
browser actions, and bridges Sony SSL calls to the ESP TLS backend. Its helper
threads and Sony callbacks need the user-mode execution environment.

These are two privilege-separated parts of the same feature, not independent
Wi-Fi plugins. Enable both VSH entries. wifi_vsh alone does not supply the full
user networking backend; wifi_menu alone cannot install the guarded kernel
patches or provide accessory power. Combining them into a single PRX would
require changing the execution/ownership model. A future single plugin-list
entry could load the other PRX as a helper without merging their privilege levels;
that packaging change is not needed for the current fixes.

wifi_native_usb.prx is the temporary kernel accessory/USB command helper.
wifi_native_session.prx supplies the temporary user networking session. The
shared psp_vnet.prx is still loaded from PSP/GAME/psp-browser, by VSH and GAME.
The separate atpro.prx GAME plugin and its adapted kd/ modules implement Ad Hoc
through the ESP over the LAN.

## Runtime configuration, not personal values compiled into firmware

- PPSSPP server: ms0:/seplugins/esp-adhoc/server.txt, containing the PC's LAN IPv4
  (or a supported resolvable hostname). This is a runtime configuration file.
- Wi-Fi: native PSP infrastructure profiles. With an empty hotspot.txt, the
  adapted aemu selection uses the first available saved profile. A non-empty
  ms0:/seplugins/esp-adhoc/hotspot.txt selects saved profiles by SSID.
- Port offset: ms0:/seplugins/esp-adhoc/port_offset.txt; the current LAN setup
  uses10000, matching PPSSPP. nickname.txt supplies the game-visible nickname.
- The ESP receives credentials from the selected PSP profile over USB when a
  connection is requested. Current firmware does not include the old ignored
  esp_link/main/wifi_credentials.h. Credentials are not included in release source.

The build's optional --server argument only writes server.txt into a local
package; it does not put the address into a PRX or ESP image. Installing updates
preserves the existing personal configuration. Sharing a configured local ZIP
also shares its server.txt, so omit --server for a generic package.

The hardcoded192.168.77.1/192.168.77.2 pair is the internal ESP/PSP USB network,
not the user's router or PC address. Upstream aemu also retains a public-server
fallback if server.txt is absent; for the current PC/LAN setup, supply server.txt.
No specific home SSID, Wi-Fi password or PC LAN address is compiled into our
current PSP/ESP build.

## Browser asset layout

Sony's updater uses F0/vsh/module and F0/vsh/resource. Our private copies live
under ms0:/PSP/GAME/psp-browser/modules/ (five original signed PRXs) and
resources/ (htmlviewer.res). The read-only brows0: device maps only its
allowlisted module/resource paths to those files, preserving Sony's loader
and its allocation/decryption bookkeeping. Legacy b0/b1 trees are migrated
by the installer with complete backups and conflict checks.
