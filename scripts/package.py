#!/usr/bin/env python3
"""Build redistributable project components. Never reads or bundles NVIDIA files."""
import argparse
import hashlib
import plistlib
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

def run(*args):
    subprocess.run([str(a) for a in args], check=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arch', choices=['arm64','x86_64','universal'], default='universal')
    parser.add_argument('--version', default='0.4.0')
    args = parser.parse_args()
    import re
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        raise SystemExit('Version must be a numeric major.minor.patch')
    architectures = ['arm64','x86_64'] if args.arch == 'universal' else [args.arch]
    build = ROOT/'.build'/('package-'+args.arch)
    if build.exists(): shutil.rmtree(build)
    resources = build/'Fanatec GFN.app/Contents/Resources'
    macos = resources.parent/'MacOS'
    resources.mkdir(parents=True); macos.mkdir()
    cache = ROOT/'.build/swift-module-cache'; cache.mkdir(exist_ok=True)
    targets = {'FanatecBridge.dylib':'bridge.c','FFBReceiver':'ffb_receiver.c'}
    for name, source in targets.items():
        parts=[]
        for arch in architectures:
            out=build/(name+'.'+arch); parts.append(out)
            command=['clang','-target',arch+'-apple-macos13.0','-O2','-Wall','-Wextra','-Werror','-ffile-prefix-map='+str(ROOT)+'=.','-framework','CoreFoundation','-framework','IOKit']
            if name.endswith('.dylib'): command += ['-dynamiclib','-install_name','@rpath/FanatecBridge.dylib']
            run(*command,ROOT/'source'/source,'-o',out)
        run('lipo','-create',*parts,'-output',resources/name)
    parts=[]
    for arch in architectures:
        out=build/('controller.'+arch);parts.append(out)
        run('swiftc','-target',arch+'-apple-macos13.0','-O','-swift-version','5',
            '-module-cache-path',cache,'-file-prefix-map',str(ROOT)+'=.',
            '-framework','Cocoa',ROOT/'source/Installer.swift',ROOT/'source/controller.swift','-o',out)
    run('lipo','-create',*parts,'-output',macos/'FanatecGFN')
    info={'CFBundleExecutable':'FanatecGFN','CFBundleIdentifier':'org.fanatecgfn.controller','CFBundleName':'Fanatec GFN','CFBundlePackageType':'APPL','CFBundleShortVersionString':args.version,'CFBundleVersion':args.version,'LSMinimumSystemVersion':'13.0','NSHighResolutionCapable':True,'CFBundleIconFile':'AppIcon'}
    (resources.parent/'Info.plist').write_bytes(plistlib.dumps(info))
    iconset = build/'AppIcon.iconset'; iconset.mkdir()
    for size in (16,32,128,256,512):
        for scale in (1,2):
            suffix = '@2x' if scale == 2 else ''
            run('sips','-z',size*scale,size*scale,ROOT/'assets/AppIcon.png','--out',iconset/f'icon_{size}x{size}{suffix}.png')
    run('iconutil','-c','icns',iconset,'-o',resources/'AppIcon.icns')
    shutil.copy2(ROOT/'LICENSE',resources/'LICENSE')
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md',resources/'THIRD_PARTY_NOTICES.md')
    for path in [resources/'FanatecBridge.dylib',resources/'FFBReceiver',macos/'FanatecGFN',resources.parent.parent]:
        run('codesign','--force','--sign','-','--timestamp=none',path)
    run('codesign','--verify','--deep','--strict',resources.parent.parent)
    dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    archive=dist/f'Fanatec-GFN-{args.version}-macos-{args.arch}.zip'
    run('ditto','-c','-k','--norsrc','--noextattr','--noacl','--keepParent',resources.parent.parent,archive)
    (dist/(archive.name+'.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
    print('Built',archive.name)

if __name__=='__main__': main()
