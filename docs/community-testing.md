# Experimental Fanatec base support

Only the CSL DD has physical driving and impulse-stop observations in this project.
The following are **source-backed experimental profiles**, not hardware certification.
Results on one model, firmware or wheel rim do not validate the others.

| Native PC-mode PID (VID 0EB7) | Profile | Constant force encoding | Project hardware status |
| --- | --- | --- | --- |
| 0020 | CSL DD / GT DD Pro / ClubSport DD family, including DD+ candidates | 16-bit | CSL DD only; other models untested |
| 0006 | Podium DD1 | 16-bit | Community test needed |
| 0007 | Podium DD2 | 16-bit | Community test needed |
| 0E03 | CSL Elite | 16-bit | Community test needed |
| 0005 | CSL Elite PS4 | 16-bit | Community test needed |
| 0001 | ClubSport V2 | 8-bit | Community test needed |
| 0004 | ClubSport V2.5 | 8-bit | Community test needed |

USB PID 0020 is shared. It does **not** establish that the device is a CSL DD.
Use the experimental checkbox for other models in this family as well. Firmware
and compatibility modes can change enumeration. Use native PC mode, not a mode
that impersonates another base. Standalone USB pedals, CSR/Porsche models and
unknown PIDs are not included in this release.

## What is implemented

- GFN matching expands its G29 match to the seven known Fanatec PIDs.
- An exact descriptor match retains the existing CSL DD input mapping. Other
  descriptors use macOS HID elements: X=steering, Z=accelerator, Rz=brake,
  optional Y=clutch, hat and the first 25 buttons. Logical ranges are normalized;
  pedal polarity follows the existing Fanatec convention (released=maximum).
- Missing, relative or duplicate required axes are rejected. This mapping is an
  experimental assumption until community tests confirm each layout and polarity.
- Force packets are bound to the selected USB product and location, so a newly
  attached second base cannot send its forces to the first.
- Exactly one known primary wheel interface must be present for physical output.
  A seven-byte vendor output field with report ID 1 or unnumbered ID 0 is required.
- Constant forces use the selected profile's 8- or 16-bit encoding. Neutral and
  stop commands use that same encoding; report IDs and lengths follow the device.
- Other PIDs require explicit experimental opt-in. Unknown layouts do not fall
  back to guessed CSL DD byte offsets. No raw Logitech output reaches the base.
- Existing 100% / unlimited defaults and immediate output remain for normal use.
  Checking **Enable experimental bases** selects **3% / 30 seconds** as a first
  test preset. The user may change these after validation; this is not a ramp.

## Community test procedure

1. Install the appropriate release for your Mac. Run **Install / repair GFN bridge**
   again after upgrading: the managed GFN copy contains its own bridge binary.
   The controller detects the older protocol and requests repair before enabling FFB.
   Close that GFN copy first; the installer refuses to modify a running copy.
2. Connect one securely mounted base in native PC mode. Leave FFB off.
3. Click **Export device diagnostics**. This reads descriptors and a current input
   snapshot. It sends no output reports, collects no serial numbers/host paths,
   and uploads nothing. Review the file before attaching it to a GitHub report.
4. Open a game in the managed GFN copy. With FFB still off, check steering center
   and both limits, each pedal independently (released and fully pressed), clutch
   if present, hat and buttons. Stop here if axes, polarity or buttons are wrong.
5. Select **Enable experimental bases**. Confirm the displayed **3% / 30 seconds**.
   Start FFB only when ready, with the base secure and its power switch accessible.
   Confirm forces return toward center instead of amplifying the steering angle.
6. Check **STOP** and the end of the 30-second session. No sustained force should
   remain. Also check pause, return to menus and leaving the game. If force remains
   or oscillates, power off the base and report the failure before further tests.
7. After the low-force checks pass, increase strength gradually as a separate test.
   Record force behavior and shutdown separately from basic input recognition.

There is no calibrated Nm limit: percentages are command scaling. The software
watchdog is not a hardware emergency stop. Do not generalize a successful driving
lap into proof of correct shutdown.

## Report results

Use the **Wheel base compatibility report** issue form. Include model, firmware,
wheel rim, pedals/connection, macOS/chip, GFN/app/game versions, diagnostics and
which checks passed or failed. Do not attach NVIDIA logs, accounts, serial numbers
or unrelated files. Report failures as well as successes. A maintainer can then
update the compatibility matrix with the exact tested configuration.

CLI diagnostics (no motor output):

```sh
"/Applications/Fanatec GFN.app/Contents/Resources/FFBReceiver" --diagnose
```

For developers, `--allow-experimental` opts a known non-0020 profile into explicit
`--live` or `--pulse-test` operation. DRY_RUN remains the default. Never add a PID
without checking its descriptor, output framing and force-resolution profile.

## Protocol sources and evidence limits

- [hid-fanatecff device definitions](https://github.com/gotzl/hid-fanatecff/blob/d83c2a216e565ac519767e4efee3b6108798c079/hid-ftec.h),
  [profile flags](https://github.com/gotzl/hid-fanatecff/blob/d83c2a216e565ac519767e4efee3b6108798c079/hid-ftec.c),
  and [force encoding / slot control](https://github.com/gotzl/hid-fanatecff/blob/d83c2a216e565ac519767e4efee3b6108798c079/hid-ftecff.c).
- [Fanatec Tuner protocol reference](https://github.com/t-hovestadt/fanatec-tuner/blob/main/PROTOCOL.md)
  reports direct ClubSport DD+ observations at PID 0020, primarily tuning/status/LEDs.
  Those observations are not proof of this bridge's FFB behavior.
- FanaBridge's PID table conflicts with the driver for some older models. This
  implementation follows the driver's explicit device table; it does not merge
  contradictory PID claims or infer compatibility from marketing names.

Tests use synthetic HID elements and a replacement USB output transport. They
verify selection, bounds, formats and stop sequencing, not the behavior of an
unavailable motor or firmware. The diagnosis on the developer's CSL DD verified
layout selection and output report 1 by reading only; no new driving test was run.
