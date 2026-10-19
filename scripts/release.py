#!/usr/bin/env python3
"""Idempotently publish artifacts built by the successful main workflow."""
import json
import os
from pathlib import Path
import subprocess

def gh(*args):return subprocess.check_output(['gh',*args],text=True).strip()
version=os.environ['VERSION'];commit=os.environ['COMMIT'];repo=os.environ['GH_REPO'];tag='v'+version
assets=sorted(Path('dist').glob('*'))
assert len(assets)==4 and all(p.suffix in ('.zip','.sha256') for p in assets)
for checksum in Path('dist').glob('*.sha256'):
    import hashlib
    digest,name=checksum.read_text().split()
    assert hashlib.sha256((Path('dist')/name).read_bytes()).hexdigest()==digest
try:
    existing=json.loads(gh('api',f'repos/{repo}/git/ref/tags/{tag}'))
except subprocess.CalledProcessError:
    existing=None
if existing:
    assert existing['object']['sha']==commit, 'Tag belongs to a different commit'
else:
    gh('api',f'repos/{repo}/git/refs','-f','ref=refs/tags/'+tag,'-f','sha='+commit)
notes=Path('release-notes.md')
notes.write_text(f'''## Fanatec GFN {version}

Native macOS controller and installer for a CSL DD bridge to GeForce NOW.

- Select **arm64** for Apple Silicon or **x86_64** for Intel.
- Includes the application icon, native installer, USB receiver and process-local bridge.
- Defaults to 100% immediate passthrough and unlimited duration; game gain controls driving strength.
- Per-archive SHA-256 checksums are attached. Core tests, coverage gates, installer fixtures and history scans passed before publication.

### Installation and compatibility

Read the [installation guide](https://github.com/{repo}/blob/{tag}/docs/installation.md) before first launch. The app is **ad-hoc signed, not Apple-notarized**. No NVIDIA software is bundled; supply your own supported client. Supported GFN versions: 2.0.86.124 and 2.0.89.154.

Physical FFB validation covers CSL DD on Apple Silicon with Assetto Corsa Competizione. Other games and physical Intel operation remain unverified. See the [validation scope](https://github.com/{repo}/blob/{tag}/docs/validation.md).

Built from `{commit}`. The automatically generated changes below describe this release's commits.
''')
try:
    gh('release','view',tag,'--json','tagName')
except subprocess.CalledProcessError:
    gh('release','create',tag,'--verify-tag','--draft','--title','Fanatec GFN '+version,'--notes-file',str(notes),'--generate-notes')
gh('release','upload',tag,*map(str,assets),'--clobber')
gh('release','edit',tag,'--draft=false','--latest')
print('Published',tag)
