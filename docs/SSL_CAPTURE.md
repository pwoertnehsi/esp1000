# PSP-side SSL/TLS capture, before ESP offloading

The user confirmed on-demand VSH power works and requested modern TLS offloading.
The first step is a passive PSP-side diagnostic. It requires no UART cable or
ESP firmware update and does not change SSL verification or connection behavior.
Rendering/JavaScript compatibility and existing browser freezes remain separate.

The virtual network driver v0.12 checks for `ms0:/ssl-capture.enable` when loaded.
If present, it writes metadata to `ms0:/ssl-capture.log` through the existing
bounded asynchronous log worker. Removing the marker disables the next session's
capture. Existing network logging still goes to `psp-vnet.log`; its first-64-
packet trace limit does not limit the independent TLS capture. No raw packet,
HTTP body, cookie, password, session key, certificate or hostname is written.
Peer IP addresses and ports are recorded. Hello prefixes exist transiently in
bounded RAM and are cleared on completion or flow cleanup.

Capture tracks four outbound TCP connections to port 443. It validates packet
checksums, joins in-order TCP segments and TLS records, suppresses retransmitted
bytes, and reads TLS ClientHello / ServerHello metadata and plaintext alerts.
It also accepts the two-byte-length SSLv2-compatible ClientHello format: this
format alone does not mean SSLv2 was negotiated. Certificate/key-exchange bodies
are skipped; encrypted payloads are never interpreted as plaintext alerts.
Protocols and alerts follow [RFC 5246](https://www.rfc-editor.org/rfc/rfc5246).

The stream decoder stops on forward TCP gaps/out-of-order data rather than
inventing a record boundary. It does not buffer/reorder missing packets. A hello
larger than the 1024-byte prefix is labelled without parsing its fields. It
records at most 48 normal metadata events per flow (first eight offered ciphers),
plus terminal events. Limits, flow evictions, disabled directions and log-queue
drops are reported. This is bounded diagnostic capture, not a full packet dump.
The PSP's UTC clock is recorded at start. EOF records require normal driver
teardown; a console crash may leave a partial log.

`SSL_TRACE` fields are numeric metadata, never raw payload:

| Event | a | b | c |
| --- | --- | --- | --- |
| CLIENT_HELLO | legacy offered version | cipher count | bit0 SNI present, bit1 supported_versions present |
| SERVER_HELLO | legacy server version | selected cipher | same extension-presence flags |
| SSL2_CLIENT_HELLO | offered version | cipher-spec count | 0 |
| CIPHER | cipher ID | zero-based index | full offered count |
| VERSIONS | highest numeric listed version | list count | 0 |
| RECORD / ENCRYPTED | record type | record version | record bytes |
| HANDSHAKE | message type | message bytes | 0 |
| ALERT | level | description | record version |
| TCP_GAP | expected sequence | observed sequence | segment bytes |
| END | new outgoing TCP bytes | new incoming TCP bytes | bit0 TX decoder stopped, bit1 RX stopped |

Version 0x0300 is SSL3, 0x0301 TLS1.0, 0x0302 TLS1.1, 0x0303 TLS1.2, 0x0304
TLS1.3. A record/legacy hello version alone does not prove the negotiated
version. VERSIONS is a literal numeric summary, not a full policy evaluation.
Fatal alert 70 is `protocol_version`, 40 `handshake_failure`, 42
`bad_certificate`, 45 `certificate_expired`, 48 `unknown_ca`. Encrypted records
or a server hello alone do not establish successful certificate verification.

Reproduce from HTTP FrogFind to `https://en.touhouwiki.net`, noting the on-screen
error. Use proven side-rail power with the external supply disconnected. Then
close the browser if possible and mount the PSP. Preserve `ssl-capture.log`,
`psp-vnet.log` and `native-wifi.log`. The local reader is:

    python3 tools/read_ssl_capture.py /run/media/tenshi/PLAYSTATION/ssl-capture.log

A local SSL failure before any TCP handshake, certificate validation result,
Sony SSL API return code, or encrypted alert reason is not exposed by this
passive observer. If the capture stops before useful wire metadata, the next
step is a narrowly guarded SSL/API error observer. No claim of full error/API
coverage or implemented TLS offloading is made by this diagnostic.

The owned 6.61 libhttp module imports candidate SSL read/write, object/BIO and
error functions. `analysis/ssl_offload_candidates.json` records name/NID hash
matches, import/export offsets and module hashes without Sony binaries. This
inventory does not prove calling conventions; call-site ABI validation is still
required before installing hooks or replacing TLS operations.

Validation: the PSP driver builds with warnings treated as errors. Eight
standalone C sanitizer programs and 57 Python tests pass. The real decoder test
covers byte-fragmented and cross-record hellos, retransmission/overlap, extension
presence, legacy ClientHello framing, plaintext/encrypted alerts, TCP gaps and
reset, bounded flow eviction, metadata budgets, prefix clearing and 10000
checksum-valid malformed payloads. Real hardware capture remains to be tested.
