# Security

Do not publish credentials, unredacted logs or exploit details in public issues. Use GitHub's private vulnerability reporting feature on the repository's Security tab when available.

This experimental local bridge is not a hardware safety system. Report issues involving invalid packet acceptance, unexpected force persistence, unsafe file replacement, leaked data or release integrity. Include affected release, macOS/CPU, reproduction steps and sanitized diagnostics.

The software accepts only a limited force protocol, checks packet freshness, bounds processing, stops on data loss and monitors the controller parent. These controls cannot guarantee a motor stop after process hangs, OS failures or power faults. Use the base's power switch if behavior is unexpected.

Never commit secrets. If a secret is exposed, revoke/rotate it first; deleting a file or rewriting Git alone does not invalidate it. Remove contaminated history and address cached references before public distribution. CI scans all reachable history with a pinned Gitleaks release and checks tracked content for common private-data patterns.

Download project releases from this repository, verify SHA-256 files and inspect provenance/check results. Ad-hoc macOS signing verifies bundle consistency but does not establish an Apple-verified publisher identity.
