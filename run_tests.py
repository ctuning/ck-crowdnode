#!/usr/bin/python

from __future__ import print_function
import subprocess
import shutil
import os
import sys
import unittest
import time
import platform

def safe_remove(fname):
    try:
        os.remove(fname)
    except Exception:
        pass

script_dir = os.path.dirname(os.path.realpath(__file__))
os.chdir(script_dir)

config_dir = os.path.join(script_dir, '.ck-crowdnode')
config_file = os.path.join(config_dir, 'ck-crowdnode-config.json')
config_file_sample_windows = os.path.join(config_dir, 'ck-crowdnode-config.json.windows.sample')
config_file_sample_linux = os.path.join(config_dir, 'ck-crowdnode-config.json.linux.sample')

files_dir = os.path.join(script_dir, 'ck-crowdnode-files')
if not os.path.exists(files_dir):
    os.makedirs(files_dir)

node_process=None
ck_dir='tests-ck-master'

def die(retcode):
    os.chdir(script_dir)
    shutil.rmtree(ck_dir, ignore_errors=True)
    shutil.rmtree(files_dir, ignore_errors=True)
    safe_remove(config_file)
    if node_process is not None:
        node_process.kill()
    exit(retcode)

safe_remove(config_file)
node_env = os.environ.copy()
if 'Windows' == platform.system():
    node_env['LOCALAPPDATA'] = script_dir
    shutil.copyfile(config_file_sample_windows, config_file)
else:
    node_env['HOME'] = script_dir
    shutil.copyfile(config_file_sample_linux, config_file)

node_process = subprocess.Popen(['build/ck-crowdnode-server'], env=node_env)

shutil.rmtree(ck_dir, ignore_errors=True)
r = subprocess.call('git clone https://github.com/ctuning/ck.git ' + ck_dir, shell=True)
if 0 < r:
    print('Error: failed to clone CK!')
    die(1)

sys.path.append(ck_dir)

import ck.kernel as ck

test_repo_name = 'ck-crowdnode-auto-tests'
test_repo_cid = test_repo_name + '::'
r = ck.access({'module_uoa': 'repo', 'data_uoa': test_repo_name, 'action': 'remove', 'force': 'yes', 'all': 'yes'})
r = ck.access({'remote': 'yes', 'module_uoa': 'repo', 'url': 'http://localhost:3333', 'quiet': 'yes', 'data_uoa': test_repo_name, 'action': 'add'})
if r['return']>0:
    print('Unable to create test repo. ' + r.get('error', ''))
    die(1)

tests_dir = os.path.join(script_dir, 'tests')

module_cfg = {
    'secret_key': 'c4e239b4-8471-11e6-b24d-cbfef11692ca',
    'platform': platform.system(),
    'repo_name': test_repo_name,
    'cid': test_repo_cid
}

def access_test_repo(param_dict):
    d = {'secretkey': module_cfg['secret_key'], 'cid': module_cfg['cid']}
    d.update(param_dict)
    r = ck.access(d)
    if r['return']>0:
        raise AssertionError('Failed to access test repo. Call parameters:\n ' + str(d) + '\nResult:\n ' + str(r))
    return r

class CkTestLoader(unittest.TestLoader):
    def loadTestsFromModule(self, module, pattern=None):
        module.ck = ck
        module.cfg = module_cfg
        module.access_test_repo = access_test_repo
        return unittest.TestLoader.loadTestsFromModule(self, module, pattern)

suite = CkTestLoader().discover(tests_dir, pattern='test_*.py')

os.chdir(tests_dir)
test_result = unittest.TextTestRunner().run(suite)

die(0 if test_result.wasSuccessful() else 1)
