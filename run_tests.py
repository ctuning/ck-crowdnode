#!/usr/bin/python

from __future__ import print_function
import subprocess
import shutil
import os
import sys
import unittest
import time

script_dir = os.path.dirname(os.path.realpath(__file__))
os.chdir(script_dir)

node_process=None
ck_dir='tests-ck-master'

def die(retcode):
    os.chdir(script_dir)
    shutil.rmtree(ck_dir, ignore_errors=True)
    if node_process is not None:
        node_process.kill()
    exit(retcode)

node_process = subprocess.Popen(['build/ck-crowdnode-server'])

shutil.rmtree(ck_dir, ignore_errors=True)
r = subprocess.call('git clone https://github.com/ctuning/ck.git ' + ck_dir, shell=True)
if 0 < r:
    print('Error: failed to clone CK!')
    die(1)

sys.path.append(ck_dir)

import ck.kernel as ck

r = ck.access({'remote': 'yes', 'module_uoa': 'repo', 'url': 'http://localhost:3333', 'quiet': 'yes', 'data_uoa': 'remote-ck-node', 'action': 'add'})

tests_dir = os.path.join(script_dir, 'tests')

class CkTestLoader(unittest.TestLoader):
    def loadTestsFromModule(self, module, pattern=None):
        module.ck = ck
        module.secret_key = 'c4e239b4-8471-11e6-b24d-cbfef11692ca'
        return unittest.TestLoader.loadTestsFromModule(self, module, pattern)

suite = CkTestLoader().discover(tests_dir, pattern='test_*.py')

os.chdir(tests_dir)
test_result = unittest.TextTestRunner().run(suite)

die(0 if test_result.wasSuccessful() else 1)
