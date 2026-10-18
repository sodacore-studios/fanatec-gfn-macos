#!/usr/bin/env python3
"""Native installer fixtures: no NVIDIA client and no hardware needed."""
from pathlib import Path
import subprocess
import unittest
ROOT=Path(__file__).resolve().parent.parent
class InstallerTests(unittest.TestCase):
    def test_patch_and_rejection_fixtures(self):
        build=ROOT/'.build';build.mkdir(exist_ok=True)
        subprocess.run(['swiftc','-module-cache-path',str(build/'swift-module-cache'),str(ROOT/'source/Installer.swift'),str(ROOT/'tests/InstallerTests.swift'),'-o',str(build/'installer-tests')],check=True)
        subprocess.run([str(build/'installer-tests')],check=True)
if __name__=='__main__':unittest.main(verbosity=2)
