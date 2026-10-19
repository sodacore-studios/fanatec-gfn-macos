# Installation

## Requirements

- macOS 13 or later; Apple Silicon or Intel Mac. Actual hardware validation is narrower: see [validation](validation.md).
- Fanatec CSL DD in red PC mode (USB VID 0EB7, PID 0020), connected directly by USB; standard two-pedal set connected to the base.
- Your own native NVIDIA GeForce NOW app and game access.
- A supported client: 2.0.86.124 or 2.0.89.154. The installer checks both version and the exact `libGeronimo.dylib` SHA-256.

Do not obtain old NVIDIA clients from untrusted mirrors. If the vendor currently supplies a newer version, it needs compatibility work before this installer can support it.

## Download

Choose the latest release ZIP matching your CPU:

- `macos-arm64`: M-series Apple Silicon.
- `macos-x86_64`: Intel.

A matching `.sha256` file accompanies each ZIP. In the download directory, verify with `shasum -a 256 -c <download-name>.zip.sha256`. Extract the ZIP and move **Fanatec GFN.app** to Applications. End users do not need Python or build tools.

## First launch

The application is locally/ad-hoc signed. It has **no Developer ID signature or Apple notarization**, so macOS may block the first launch. If you trust the downloaded release and have checked its checksum, use the per-app **Open Anyway** option in System Settings → Privacy & Security after attempting to open it. Managed Macs may disallow this. Do not disable Gatekeeper or SIP globally.

## Prepare the GFN copy

1. Close any previously managed GFN copy and stop force feedback.
2. Launch Fanatec GFN and click **Install / repair GFN bridge…**.
3. Select the original `GeForceNOW.app`, usually in `/Applications`.
4. Review and accept the installation dialog.

The installer validates the client before copying. It creates:

```text
~/Applications/Fanatec GFN/GeForceNOW Fanatec.app
```

It adds the bridge load command into unused Mach-O header space and signs the copy locally, preserving the original entitlements plus per-app library-validation relaxation. It verifies the resulting signature. No global security setting is changed. The original NVIDIA app is unchanged.

An existing managed copy is retained as `Previous-<random-id>.app`. Unmanaged destinations are never overwritten. A failure before replacement removes staging files; a failure during replacement attempts to restore the previous copy. Backups can be removed manually once the new copy is verified and closed.

## Start playing

Connect and power on the wheel in red PC mode. Use **Open GeForce NOW** in the controller, authenticate normally with NVIDIA and launch the game. Configure the Logitech G29 axes/buttons. Enable force feedback separately with the controller's Start button.

The bridge exists only inside the managed GFN process. It does not create a system-wide G29 device and does not enable arbitrary native macOS games.

## Build from source

Install Apple's Command Line Tools and use Python 3:

```sh
python3 scripts/package.py --arch universal --version 0.4.0
```

The ZIP appears in `dist/`. Build products are ignored by Git. The build does not need or include a NVIDIA installation. `source/build_controller.py` is a compatibility entry point for the same packager. `source/build.py` is a legacy developer-only local patching tool with checkout-local paths; use the native installer for normal operation.

## Uninstall / rollback

Stop force feedback and quit both apps. Remove Fanatec GFN.app and the managed directory under `~/Applications/Fanatec GFN/`. Remove `~/Library/Application Support/FanatecGFN/` if logs are no longer needed. Continue using the original NVIDIA app. No system extension or background service needs removal.
