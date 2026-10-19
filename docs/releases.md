# CI and release operations

Every branch push and every pull request to main runs **CI and release**:

1. Audit tracked content and all reachable authored Git history for common private-data patterns and author email hygiene. On PRs, this job checks out the submitted head; GitHub-generated temporary merge metadata is outside the source history. Build/test jobs still exercise the PR merge result.
2. Run Gitleaks 8.30.1, verified against a pinned archive SHA-256, on all reachable history.
3. On both Apple Silicon (`macos-15`) and Intel (`macos-15-intel`): run isolated unit/runtime tests, coverage gates, native installer fixtures and the application build.
4. Verify signatures and the release ZIP's exact component allowlist. Reject build-machine home paths in bundled files.
5. Publish coverage JSON as workflow artifacts.
6. Aggregate all prerequisite results into the required **CI** check.
7. Only for a successful main push: create `v0.4.<workflow-run-number>`, create release notes, upload both architecture ZIPs and SHA-256 files, then publish the release.

Version numbers increase with workflow runs; PR runs can leave gaps. Rerunning the same workflow keeps the same version and verifies that an existing tag points to the same commit. Downloads are uploaded to a draft before it is published. Failed prerequisites prevent the release job entirely. A failed upload can leave a tag/draft; rerun the successful commit's workflow to complete it.

The workflow uses SHA-pinned Actions and read-only permissions by default. Only the release job receives `contents: write`; it uses the ephemeral GitHub token. No personal access token or Apple signing credential is required. PR code never runs in a privileged `pull_request_target` workflow.

Main must retain the required **CI** status check, up-to-date checks and force-push/deletion restrictions. Maintainers should use pull requests for changes. The repository owner can change protection settings; protections are not immutable.

## Release verification

Check the workflow for the exact release commit, download the matching ZIP and checksum, and verify SHA-256 before extracting. The ZIP contains only project code, icon and notices. NVIDIA files are installed locally by the user and must never be uploaded as assets.

Apple notarization/Developer ID signing is not configured. Releases use ad-hoc signing, so macOS may require a per-app first-launch exception. Linux/Windows binaries are not produced because the implementation requires macOS IOKit/AppKit APIs. Cross-platform support would be a separate port, not another packaging target.
