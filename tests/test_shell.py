
import unittest

# The following variables are initialized by test runner
ck=None                 # CK kernel
cfg=None                # test config
access_test_repo=None   # convenience function to call the test repo without the need to specify its UOA and secretkey.
                        # You just need to provide 'action' and the action's arguments

class TestPushPull(unittest.TestCase):

    def test_shell(self):
        cmd = 'dir C:\\' if 'Windows' == cfg['platform'] else 'ls -l /etc/'
        r = access_test_repo({'action': 'shell', 'cmd': cmd})
        self.assertIn('stdout', r)
        self.assertIn('return_code', r)
        self.assertIn('stderr', r)
