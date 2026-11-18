# Gran Turismo car transfer milestone

On 2026-11-18 the user successfully transferred cars from a physical PSP E1000
running Gran Turismo to PPSSPP on the same LAN through the ESP32-S3 backend.
This is a user-confirmed game interoperability result beyond the UDP peer test.
A race and prolonged recovery testing remain unverified.

The launch succeeded after selecting ARK Extra Memory Auto. The installed
configuration also retains the title-specific Auto / cache overrides prepared
for the owned game UCES01245; this run does not isolate which individual setting
was necessary. ESP20 and the existing saved native connection were retained.

Before accessory-power changes, logs were copied to the ignored local directory
`milestones/adhoc-granturismo-pass`, and the exact installed Ad Hoc modules and
configuration were preserved with SHA256 hashes in
`artifacts/adhoc-granturismo-pass`. These archives contain private configuration
and are not release artifacts. The browser exit problem remains unresolved.

After ESP22 corrected overlapping USB host FIFO storage, the user reported a
complete playable multiplayer race with no issues. This extends the milestone
from car transfer to gameplay. The passing setup retained ESP21's mapping/TCP
recovery and the existing Ad Hoc modules. Long-term stress and additional games
remain untested. PSP logs available before the VSH power update are archived
locally in `milestones/adhoc-race-pass-20261118`.
