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

## Directory-independent test player

The first hardware capture recorded RADIO_STATE slot 4/value 0 and the patched
radio consumer bit. The ESP state integration was exercised, but neither Sony
player produced audio: Player II kept scanning and Jazz did not respond to its
switch. This does not establish the condition of their directory services.
An ordinary PC browser cannot validate PSP plugin-dependent controls.

Two community alternatives with primary project pages are
[PSPRadio.ru](https://pspradio.ru/player.html) (lists a 2025 release) and
[My Playlist](https://github.com/Vasniktel/My-Playlist) (older, supports editing
stations). No live playback through either project has been verified here.

For a controlled stream test, the original radio_player/index.html uses Sony's
native radio object with direct URLs. It is ES3-style HTML/JavaScript with no
external UI assets or station directory. Play and Stop call the native APIs;
status is displayed numerically with Sony's documented state label.
The default and alternate HTTP presets are transport tests, not verified live
endpoints. The HTTPS MP3 URLs are listed on
[SomaFM's official Groove Salad stream page](https://somafm.com/groovesalad/directstreamlinks.html).
Direct server addresses can change; the URL input permits replacement.

Prepare a private launcher from an existing owner-supplied Sony PRS:

```sh
python3 tools/prepare_radio_player.py \
  --template /run/media/tenshi/PLAYSTATION/PSP/RADIOPLAYER/InternetRadioPlayerII.prs \
  --output build/direct-radio
```

Copy the generated PSP/RADIOPLAYER contents to the same directory on the PSP.
Launch ESP1000 Direct Radio from XMB Internet Radio, allow its embedded plugin,
then select a preset and press Play. If HTTP fails, try HTTPS and report both the
Play return and native status. Report whether a connection chooser appears too.
This player has not yet been hardware-validated; file-origin plugin/network
behavior is part of this test, not an assumed success.

The preparation tool changes only bounded metadata strings in a copy, preserving
container offsets, lengths of unrelated sections and the original private icon.
It refuses malformed tables, duplicate properties and replacements that do not
fit. The original PRS is never edited. Its Sony icon/container are not distributed
in the repository: generated private packages stay under ignored build/.

If a player stays on its initial text and Play/Stop do not respond, first check
browser Tools > Settings > View Settings > JavaScript is On, then reload. Sony
lists this setting in its [browser settings manual](https://manuals.playstation.net/document/en/psp/current/network/browser/settings.html).
The embedded-plugin permission prompt alone does not confirm that page scripts
are running. The local page includes a noscript message. Its former Test JavaScript button
was removed after the user reported that its label did not update despite
successful native Play calls. The button was not a reliable scripting diagnostic
on this browser; a native Play return of 0 already confirms script execution.

The user confirmed JavaScript is On and the native-plugin prompt appeared, yet
Play/Stop did not change the initial page. The local page now uses named form
control values for all inputs/status, rather than DOM node creation and ID
lookup. It binds Sony's global psp object directly, removes method-type
introspection, and uses the documented deferred initialization pattern (1500 ms).
The form exists before the bootstrap script writes its JavaScript-running marker.
An Initialize button retries native binding after permission; failures clear the
binding and show an explicit error. Play/Stop handlers use prefixed names and
report that the click was received before calling native APIs. Optional stream
metadata failure no longer hides the basic native status.

This addresses compatibility assumptions in the test UI; it does not establish
that a DOM method caused the hardware failure or that audio now works. Its host
JavaScript test deliberately supplies neither getElementById/createTextNode nor
document.psp, and covers missing-plugin failure, retry and title-query failure.
No firmware or browser preference change is required for this page update.

## Queued request versus playback

Sony's sysRadioPlayStream return 0 accepts a queued command; it does not prove
a stream has connected or decoded. The direct player now sets its sub-volume
after the Play command, matching the ordering in Sony's examples, rather than
only during initialization. Whether that ordering caused this hardware silence
remains unconfirmed.

Native status and buffer are now near the controls, queried immediately after
Play and by a manual Status button as well as the timer. Documented status
values are 0 idle, 1 playing, 2 fetching playlist, 3 parsing playlist, 4 connecting,
and -1 error. Buffer is the native 0-255 indicator of compressed/decoded audio
accumulation, not a percentage or a guarantee of audible output. Optional
metrics remain guarded so unavailable methods do not hide the base status.
The control test checks Play-then-volume ordering and status/error labels.

Source: Sony-authored [Internet Radio Player Development Guide](https://manuals.plus/m/9afcb4ba41412facb1e1f5882b9432a60f208da3632938ae3282dd313391e4c4),
sysRadioPlayStream, sysRadioGetPlayerStatus and sysRadioGetStreamBufferLevel.

### Queued request stays idle

The hardware test accepted `sysRadioPlayStream` (return 0) but continued reporting
player state 0 and stream buffer 0. This is not evidence of a successful stream
connection. The player now includes a monotonically increasing Sample number so
manual Status presses and timer updates are visible even when native values stay
unchanged.

Debug builds include `radioshack_plugin_module` as owner 8 in
`browser-sony-thread.log`. `RADIO_WORKERS` explicitly records the native command
dispatcher (`SceRsJsDispatcherBg`) and audio worker (`SceRsAudioThread`) UIDs;
zero means no matching thread was present in that complete enumeration. Their
SONY lines record scheduling state and wait object. This is passive observation,
not a worker restart or a playback fix. The trace runs during a radio-only
session as well as an ordinary browser session, and stops writing during USB
storage mode. It records no stream URLs or credentials.
