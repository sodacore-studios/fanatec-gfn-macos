#!/usr/bin/env python3
"""Compatibility entry point for developers; release users do not need Python."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).resolve().parent.parent/'scripts/package.py'),run_name='__main__')
