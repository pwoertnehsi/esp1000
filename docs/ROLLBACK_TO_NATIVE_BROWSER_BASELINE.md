# Native browser baseline rollback

Restored on2026-11-18 after repeated Browser exit hangs during USB recovery
experiments. Baseline source is38c3665, the point where the user confirmed
native Browser launch, network page loading, saved-profile use and exit worked.
Runtime artifacts are PSP package v16 and ESP firmware v15.

The current PSP logs and replaced runtime binaries are preserved locally under
artifacts/rollback-to-psp16-esp15 (ignored). All nine installed PSP files match
the original v16 package manifest on separate readback. The bridge flashed the
original ESP15 bootloader, partition table and application successfully and
reported matching SHA256 values. PSP profiles and plugin configuration were
retained. Source restoration was validated with eight C sanitizer suites and
44 Python tests. Hardware validation after this rollback remains pending.

Experimental recovery commits remain available on
archive/recovery-before-rollback-ae4a2f6. The source on main again matches the
baseline except this rollback record. Do not mix newer recovery helpers with
this baseline; their control ABI differs.

This restores previously usable behavior; it does not fix the original USB
dropout or reset requirement. It also predates the Display Connection Status
security-enum correction, so avoid that Browser command on this baseline.

Future recovery work must remain separate from the baseline installation until
tested as a complete package with connection, page load, disconnect, reconnect
and Browser exit.
