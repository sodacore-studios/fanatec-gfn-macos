# Contributing

Use English for code comments, documentation, issues and pull requests. Keep changes narrowly scoped and explain user-visible behavior and validation.

Before opening a pull request, run:

```sh
python3 scripts/test.py
python3 scripts/coverage.py
python3 scripts/installer_tests.py
python3 scripts/audit.py
python3 scripts/package.py --arch universal --version 0.4.0
```

The C and Swift runtime uses only Apple system frameworks. Python is used for build and test orchestration. Add deterministic tests for protocol and installer changes. Tests must never access a physical motor, launch a game or depend on credentials, a NVIDIA installation or a network service.

Use a GitHub noreply email for commits. Do not commit logs, personal paths, device/user identifiers, environment files, credentials, proprietary binaries or internal project information. Before sharing diagnostics, inspect and redact them. The full reachable Git history is scanned in CI.

Open a pull request for main. The aggregate **CI** check must pass before merge; releases are created only after all checks succeed on main. See [release operations](docs/releases.md).

A new GFN version requires protocol/client review and physical validation. Do not simply remove the version or checksum gate to make a build succeed. Report compatibility evidence without publishing vendor binaries or credentials.
