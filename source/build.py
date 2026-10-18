#!/usr/bin/env python3
"""Build/refresh a local GFN bridge. Never edits /Applications/GeForceNOW.app."""
import argparse
import hashlib
import plistlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "source"
ORIGINAL = Path("/Applications/GeForceNOW.app")
APP = ROOT / "GeForceNOW Fanatec FFB.app"
EXPECTED_VERSION = "2.0.86.124"
EXPECTED_GERONIMO = "b7d936c4e51601a08c0a101682801e66232e1eceece5a4ace47386390264906f"
KNOWN_VERSIONS = {
    EXPECTED_VERSION: EXPECTED_GERONIMO,
    '2.0.89.154': 'd82b1d3b26d9d998b78b7879f879daf9df6694e5e3f63767ebcb6cf6982c005f',
}

def run(*args):
    subprocess.run(args, check=True)

def add_dylib(binary, name):
    data = bytearray(binary.read_bytes())
    if data[:4] != b"\xca\xfe\xba\xbe":
        raise RuntimeError("Expected universal Mach-O")
    slices = struct.unpack_from(">I", data, 4)[0]
    for index in range(slices):
        _, _, offset, size, _ = struct.unpack_from(">IIIII", data, 8 + index * 20)
        magic, = struct.unpack_from("<I", data, offset)
        assert magic == 0xfeedfacf
        count, total = struct.unpack_from("<II", data, offset + 16)
        cursor = offset + 32
        first_section = size
        already_loaded = False
        for _ in range(count):
            cmd, length = struct.unpack_from("<II", data, cursor)
            assert length >= 8 and cursor + length <= offset + 32 + total
            if cmd == 0xc:
                name_offset, = struct.unpack_from('<I', data, cursor + 8)
                existing = bytes(data[cursor + name_offset:cursor + length]).split(b'\0')[0]
                already_loaded |= existing == name.encode()
            if cmd == 0x19:  # LC_SEGMENT_64
                sections, = struct.unpack_from("<I", data, cursor + 64)
                for section in range(sections):
                    location, = struct.unpack_from("<I", data, cursor + 72 + 80 * section + 48)
                    if location:
                        first_section = min(first_section, location)
            cursor += length
        if already_loaded:
            continue
        raw = name.encode() + b"\0"
        length = (24 + len(raw) + 7) & ~7
        command = struct.pack("<IIIIII", 0xc, length, 24, 0, 0, 0) + raw
        command += b"\0" * (length - len(command))
        if cursor + length > offset + first_section or any(data[cursor:cursor + length]):
            raise RuntimeError("No unused load-command padding; refusing to patch")
        data[cursor:cursor + length] = command
        struct.pack_into("<II", data, offset + 16, count + 1, total + length)
    binary.write_bytes(data)

def main():
    global APP
    parser = argparse.ArgumentParser()
    parser.add_argument('--refresh', action='store_true', help='Update the bridge in the stopped test app')
    parser.add_argument('--app-name', default=APP.name, help='Local test .app name in this output folder')
    parser.add_argument('--base-app', default=str(ORIGINAL), help='Installed GFN app to copy when creating a new test app')
    args = parser.parse_args()
    if Path(args.app_name).name != args.app_name or not args.app_name.endswith('.app'):
        raise ValueError('app-name must be a local .app filename')
    APP = ROOT / args.app_name
    if len(str(ROOT / 'ffb.sock').encode()) >= 104:
        raise RuntimeError('Checkout path too long for macOS UNIX socket; use a shorter directory')
    base = APP if args.refresh else Path(args.base_app).resolve()
    info = plistlib.loads((base / "Contents/Info.plist").read_bytes())
    base_version = info['CFBundleShortVersionString']
    assert base_version in KNOWN_VERSIONS, 'Recheck new GFN version first'
    lib = base / "Contents/Frameworks/libGeronimo.dylib"
    assert hashlib.sha256(lib.read_bytes()).hexdigest() == KNOWN_VERSIONS[base_version]
    if APP.exists() and not args.refresh:
        raise RuntimeError("Test app already exists; refusing to overwrite it")
    if args.refresh:
        running = subprocess.check_output(['ps', '-axo', 'comm='], text=True)
        if str(APP) in running or str(ROOT / 'FFB-Test') in running:
            raise RuntimeError('Close test app and FFB receiver before refresh')
    # Preserve the app's original permissions and add only per-app library loading.
    entitlements = subprocess.check_output(["codesign", "-d", "--entitlements", "-", "--xml", str(base)])
    values = plistlib.loads(entitlements)
    values["com.apple.security.cs.disable-library-validation"] = True
    entfile = ROOT / "test-app-entitlements.plist"
    entfile.write_bytes(plistlib.dumps(values))
    if not args.refresh:
        run("ditto", str(base), str(APP))
    bridge = APP / "Contents/Frameworks/FanatecBridge.dylib"
    run("clang", "-arch", "arm64", "-arch", "x86_64", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-dynamiclib", "-framework", "CoreFoundation", "-framework", "IOKit",
        '-DBRIDGE_LOG=' + json.dumps(str(ROOT / 'bridge.log')), str(SOURCE / "bridge.c"), "-o", str(bridge))
    run("codesign", "--force", "--sign", "-", "--timestamp=none", str(bridge))
    add_dylib(APP / "Contents/MacOS/GeForceNOW", "@executable_path/../Frameworks/FanatecBridge.dylib")
    run("codesign", "--force", "--sign", "-", "--options", "runtime", "--timestamp=none",
        "--entitlements", str(entfile), str(APP))
    run("codesign", "--verify", "--deep", "--strict", str(APP))
    run('clang', '-arch', 'arm64', '-arch', 'x86_64', '-Wall', '-Wextra', '-Werror',
        '-framework', 'CoreFoundation', '-framework', 'IOKit',
        '-DFFB_SOCKET=' + json.dumps(str(ROOT / 'ffb.sock')),
        str(SOURCE / 'ffb_receiver.c'), '-o', str(ROOT / 'FFB-Test'))
    print("Built:", APP)
    print("App captures force data. Physical output requires a separately started FFB-Test --live.")

if __name__ == "__main__":
    main()
