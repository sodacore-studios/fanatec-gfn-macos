# Compatibility and validation

## Confirmed physical setup

- Apple Silicon M3, macOS 27.0.1.
- Fanatec CSL DD, red PC mode, VID 0EB7 / PID 0020.
- Standard two-pedal set connected to the base.
- Native GFN 2.0.89.154; Assetto Corsa Competizione.

The cloud session recognized a G29 identity. Steering, accelerator, brake and button inputs were observed. Opposing short motor impulses were confirmed, followed by staged driving tests. At 50%, the user confirmed distinct, appropriate forces. During the 120-second 100% test, the receiver processed 11,824 packets: 10,185 translated and 1,639 rejected/unsupported, with zero reported USB write errors. The user confirmed the force and driving feel.

These are functional observations, not a calibrated torque measurement or endurance certification. The later unlimited/immediate-output behavior was separately checked with dry-run tests. The new downloadable installer/controller packaging is validated separately from that original driving session; packaging does not imply a repeated physical test on every build.

## Automated checks

- Input report length, report ID, null handling, steering bounds, independent pedals, button masking and pedal swap.
- Every force level across the diagnostic ramp range, sign/inversion, clamp, stop forms, malformed/unsupported packets, invalid gains and null inputs.
- Packet metadata, freshness boundaries, future timestamps and watchdog boundaries.
- DRY_RUN integration: full gain on first packet, stale/unknown rejection, watchdog stop, timed exit, unlimited session, explicit stop, parent exit and socket cleanup.
- Native installer: thin/universal Mach-O fixtures, idempotency, malformed/truncated commands, insufficient padding and unsupported-version rejection.
- Address/undefined-behavior sanitizers for the portable C tests.
- Coverage gates: at least 95% lines and 90% branches for `report_map.h`, `ffb_protocol.h` and `ffb_wire.h`. Reports are CI artifacts. This scope intentionally excludes IOKit, GUI and hardware coverage.
- Builds and signature checks on Apple Silicon and Intel macOS runners; release archives contain only project-owned components.

Tests never start GFN, invoke `--live` or send motor commands. Installer tests use synthetic fixtures; proprietary NVIDIA binaries are not installed in CI.

## Not yet verified

- Other games, other Fanatec bases, different wheels/pedals and accessory layouts.
- Physical Intel Mac operation or runtime on every supported minimum OS version.
- Hot-plug, sleep/wake, prolonged endurance, all crash scenarios.
- Non-constant Logitech effects, LEDs, complete configuration command support.
- Newer GFN versions and long-term compatibility with NVIDIA updates.

A game listing Logitech G29 support may work through the same bridge, but each game's inputs and force behavior should be checked before calling it supported.
