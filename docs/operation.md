# Operation

## Controls

| Control | Behavior |
|---|---|
| Install / repair | Validate and create a separate GFN copy; disabled while force feedback is active |
| Open GeForce NOW | Open the managed copy |
| Passthrough | 10, 25, 50, 75 or 100 percent; default 100 |
| Duration | Unlimited (default), 5, 15, 30 or 60 minutes |
| Start force feedback | Start the physical USB receiver explicitly |
| STOP | Request receiver termination and stop commands |

At 100%, the bridge does not attenuate the recognized game force. This is not a torque measurement or a promise of the base's rated Nm. The game and base settings still determine the driving force. Output starts immediately: there is no normal-session ramp. Settings cannot be changed while the receiver is running; stop first.

Unlimited has no session timer. The data-loss watchdog remains active: after more than 250 ms without a recognized force/stop packet, the receiver sends stop. Fresh valid packets resume force. Invalid/unsupported packets never keep the watchdog alive. Normal closing of the controller stops the child; the child also checks its parent process.

Securely mount the base and start conservatively on a new setup. If forces pull away from the expected direction or oscillate, power off the base. The watchdog is software, **not a hardware emergency stop**: an OS failure, hung receiver or forced kill can prevent stop delivery.

## Logs and runtime files

```text
~/Library/Application Support/FanatecGFN/bridge.log
~/Library/Application Support/FanatecGFN/session.log
~/Library/Application Support/FanatecGFN/ffb.sock
```

The socket is local to the user. Logs can contain process IDs, paths, device metadata and input/force data; inspect and redact them before sharing. `session.log` is replaced on each start. No credentials are required or stored by the bridge. NVIDIA manages its own sign-in data in its app.

## Troubleshooting

| Symptom | Check / action |
|---|---|
| Unsupported GFN version or checksum | Installer intentionally refuses it. Report version and sanitized diagnostic details; do not bypass the check. |
| Wheel absent in game | Red PC mode, USB connection, managed GFN copy, correct game controller settings. |
| Inputs work but no FFB | Start FFB in controller; check game FFB settings and `session.log`. Other effect formats may be unsupported. |
| Bridge missing after update | Stop FFB and close GFN. Run Install / repair with a supported original client. An unknown version needs review. |
| Socket already exists | Close all controllers/receivers first. Only remove a stale `ffb.sock` after confirming no receiver runs. |
| Data pause / force stops | Check game streaming/session state. Valid data resumes output automatically; this is independent of the duration setting. |
| Device open/write error | Check cable, power and mode. Close competing wheel utilities. Power off the base if behavior is unexpected. |
| macOS blocks opening | Follow the per-app first-launch instructions; enterprise policy may prevent local unsigned apps. |

Do not run multiple controllers or managed GFN instances against the same wheel. Hot-plug recovery and sleep/wake are not validated. Stop FFB before disconnecting the device or putting the Mac to sleep.

## Testing another game

G29 support is a useful prerequisite, not a compatibility guarantee. Check axes, pedals, buttons and force direction at low strength, then sustained output. Record the game, client version, hardware and observed effects. Only ACC currently has a confirmed physical driving result; the bridge implements constant force and selected stop commands, not every Logitech effect.
