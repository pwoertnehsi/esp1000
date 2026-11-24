# Native Internet Radio over ESP1000

The stock Internet Radio entry launches Sony's browser in radio mode, then asks
permission to load its embedded radio plugin. The browser UI and native playback
module are separate APCTL consumers. The existing `.prs` files are launchers:
their `radioplayer_url` points at a hosted HTML/JavaScript player, rather than
containing that player or a station directory locally.

The supplied players point to Sony's PLAYER_II and os_JazzOrNot pages. The former
identifies Icecast as its directory and the latter SHOUTcast. Those launchers
and the downloaded player pages are left unchanged. A loaded player UI does not
prove its station directory, stream URL or audio format is working.

## ESP integration

Owned 6.61 radioshack_plugin.prx is radioshack_plugin_module, version 1.1, text
size 0x21358. Its APCTL table at +0x1e334 imports GetState (NID 5DEAC81B) through
+0x1dd50. The local call at +0x77dc reaches it independently of the browser's
APCTL imports. Previously it queried the physical APCTL provider, which the ESP
backend does not connect. Now only this consumer's guarded import is redirected
to the live ESP facade. Other clients and the physical APCTL provider remain
unchanged. Kernel/user hook ABI 4 requires matched wifi_vsh/wifi_menu modules.

A live browser or radio client retains the network/module/accessory-power lease;
shared HTTP alone does not. Explicit disconnect still uses the normal cleanup
path. No ESP firmware change, flash write, credential change or media decoder
replacement is part of this implementation.

## Hardware test and debug records

Restart VSH after installing the matching debug XMB modules. Open an existing
Internet Radio player, connect using the saved profile, allow its embedded plugin,
and select a station. Record whether stations are listed, the player indicates
buffering or an error, and whether audio plays. Then close the player and check
that accessory power is released. Hardware playback is not yet confirmed.

In debug native-wifi.log, RADIO_STATE reports transitions from the radio's own
state query: slot 4 with value 0 means the radio was given GOT_IP successfully;
slot 0 means disconnected. Values are hexadecimal. Identical repeated polls are
not logged. In wifi-vsh.log, the NATIVE_BROWSER consumer mask gains bit 0x10 when
the radio import has been patched. A hook failure is different from a working
hook whose selected stream is inaccessible or unsupported. Production omits
these diagnostics.

Host validation covers exact radio import guards (including refusal of foreign
stubs), matching hook ABI, live state/error output, unchanged output bounds,
radio-only power retention and release after the owner disappears. A successful
build or host test is not evidence that a particular live station plays.

## References

- Sony's [Internet Radio manual](https://manuals.playstation.net/document/en/psp/current/network/internetradio/index.html).
- [ticky/psp-radio](https://github.com/ticky/psp-radio) documents the special browser
  mode and application/x-psp-extplugin API; its browser emulator is not installed
  or used by this PSP implementation.
