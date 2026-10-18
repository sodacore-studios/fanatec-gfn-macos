#!/usr/bin/env python3
"""Check tracked content and optionally all reachable history for private-data patterns.

This complements Gitleaks and human review; it cannot prove the absence of every secret.
Matches are reported by location/rule, never by secret value.
"""
import argparse
import re
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
RULES={
    'personal-home-path':rb'(?:/Users/|/home/)[A-Za-z0-9_.-]+/',
    'private-ip':rb'(?<![\d.])(?:10(?:\.\d{1,3}){3}|192\.168(?:\.\d{1,3}){2}|172\.(?:1[6-9]|2\d|3[01])(?:\.\d{1,3}){2})(?![\d.])',
    'internal-host':rb'https?://[^\s/]+\.(?:internal|local|corp)(?:[/:\s]|$)',
    'private-key':rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
}
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
def check(path,data):
    failures=[]
    name=Path(path).name.lower()
    if name.startswith('.env') or name.endswith(('.pem','.key','.p12','.pfx','.log','.sock')):
        failures.append((path,'excluded-file'))
    if '.app/' in path or name.endswith(('.dylib','.exe')):failures.append((path,'binary-runtime-file'))
    for rule,pattern in RULES.items():
        if re.search(pattern,data):failures.append((path,rule))
    return failures

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--history',action='store_true');args=parser.parse_args()
    failures=[]
    for path in git('ls-files','-z').decode().split('\0'):
        if path and (ROOT/path).is_file():failures+=check(path,(ROOT/path).read_bytes())
    if args.history:
        objects={}
        for commit in git('rev-list','--all').decode().splitlines():
            metadata=git('show','-s','--format=%ae%n%ce',commit).decode().splitlines()
            for address in metadata:
                if not address.endswith('@users.noreply.github.com'):
                    failures.append((commit[:12],'non-noreply-author-email'))
            for entry in git('ls-tree','-rz',commit).split(b'\0'):
                if not entry:continue
                info,path=entry.split(b'\t',1);kind,oid=info.split()[1:]
                if kind==b'blob':objects[(oid.decode(),path.decode())]=True
        for oid,path in objects:failures+=check(path,git('cat-file','blob',oid))
    for path,rule in sorted(set(failures)):print(f'FAIL {rule}: {path}')
    if failures:raise SystemExit(1)
    print('PASS: tracked-content/privacy audit'+(' including reachable Git history' if args.history else ''))
if __name__=='__main__':main()
