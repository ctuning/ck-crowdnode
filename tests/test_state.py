
import unittest

# The following variables are initialized by test runner
ck=None                 # CK kernel
cfg=None                # test config
access_test_repo=None   # convenience function to call the test repo without the need to specify its UOA and secretkey.
                        # You just need to provide 'action' and the action's arguments
files_dir = None        # Path to files from config

class TestPushPull(unittest.TestCase):

    def test_state(self):
        r = access_test_repo({'action': 'state'})
        cfg_ = r['cfg']
        self.assertIn('path_to_files', cfg_)
        self.assertEqual(files_dir, cfg_['path_to_files'])
        self.assertIn('return', r)
        self.assertEqual(0, r['return'])

