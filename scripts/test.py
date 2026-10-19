#!/usr/bin/env python3
"""Isolated deterministic macOS checks. No live USB or NVIDIA process access."""
import ast
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
ROOT=Path(__file__).resolve().parent.parent
SOURCE=ROOT/'source'
def run(*args,**kwargs):
    return subprocess.run([str(a) for a in args],check=True,**kwargs)
def stop(process):
    if process.poll() is None:process.terminate()
    return process.communicate(timeout=5)[0]
class CoreTests(unittest.TestCase):
    def check_core(self,name):
        with tempfile.TemporaryDirectory(prefix='fgfn-') as directory:
            binary=Path(directory)/name
            run('clang','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',SOURCE,ROOT/'tests'/('test_'+name+'.c'),'-o',binary)
            run(binary)
    def test_report_mapping(self):self.check_core('report_map')
    def test_force_translation(self):self.check_core('force_protocol')
    def test_wire_validation(self):self.check_core('wire_protocol')
    def test_source_syntax(self):
        for folder in ('source','scripts'):
            for path in (ROOT/folder).glob('*.py'):ast.parse(path.read_text())
class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='fgfn-',dir='/tmp')
        self.addCleanup(self.temporary.cleanup)
        self.tmp=Path(self.temporary.name);self.socket=self.tmp/'ffb.sock';self.receiver=self.tmp/'receiver'
        run('clang','-Wall','-Wextra','-Werror','-framework','CoreFoundation','-framework','IOKit','-DFFB_SOCKET='+json.dumps(str(self.socket)),SOURCE/'ffb_receiver.c','-o',self.receiver)
    def start(self,*args):
        p=subprocess.Popen([str(self.receiver),*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        self.addCleanup(stop,p)
        deadline=time.monotonic()+3
        while not self.socket.exists():
            if p.poll() is not None:self.fail('Receiver exited before socket: '+p.stdout.read())
            if time.monotonic()>deadline:self.fail('Receiver did not bind')
            time.sleep(.01)
        return p
    def test_invalid_arguments(self):
        for args in (['--session','--seconds','abc'],['--session','--seconds','-1'],['--seconds','0'],['--session','--seconds','3601'],['--session','--gain','1.01'],['--session','--gain','0.1junk']):
            with self.subTest(args=args):
                result=subprocess.run([str(self.receiver),*args],capture_output=True,text=True)
                self.assertEqual(result.returncode,2)
    def test_first_packet_watchdog_and_rejection(self):
        run('clang','-Wall','-Wextra','-Werror','-I',SOURCE,ROOT/'tests/send_packets.c','-o',self.tmp/'sender')
        p=self.start('--session','--gain','1.0','--seconds','2')
        run(self.tmp/'sender',self.socket)
        output=p.communicate(timeout=5)[0]
        self.assertEqual(p.returncode,0,output)
        for expected in ('mode=DRY_RUN','source=ff level=65279 command=01','watchdog stop','received=4 translated=1 rejected=3'):
            self.assertIn(expected,output)
        self.assertFalse(self.socket.exists())
    def test_unlimited_and_explicit_stop(self):
        p=self.start('--session','--seconds','0')
        time.sleep(.1)
        self.assertIsNone(p.poll())
        output=stop(p)
        self.assertEqual(p.returncode,0,output)
        self.assertIn('unlimited until stop',output);self.assertIn('finished',output)
        self.assertFalse(self.socket.exists())
    def test_parent_exit_cleanup(self):
        logfile=self.tmp/'parent-exit.log'
        code='''import os, subprocess, sys, time
p=subprocess.Popen([sys.argv[1], '--session', '--seconds', '0', '--parent-pid', str(os.getpid())], stdout=open(sys.argv[2], 'w'), stderr=subprocess.STDOUT)
for _ in range(200):
    if os.path.exists(sys.argv[3]): break
    time.sleep(.01)
os._exit(0)
'''
        run(sys.executable,'-c',code,self.receiver,logfile,self.socket)
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            output=logfile.read_text() if logfile.exists() else ''
            if 'finished' in output:break
            time.sleep(.01)
        self.assertIn('controller exited; stopping',output)
        self.assertIn('finished',output);self.assertFalse(self.socket.exists())
if __name__=='__main__':
    if sys.platform!='darwin':raise SystemExit('These runtime checks require macOS')
    unittest.main(verbosity=2)
