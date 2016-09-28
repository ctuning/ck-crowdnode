
import unittest

class TestPushPull(unittest.TestCase):

    def check(self, r):
        self.assertEqual(0, r['return'], str(r))

    def test_shell(self):
        cmd = 'dir C:\\' if 'Windows' == platform else 'ls -l /etc/'
        r = ck.access({'cid': 'remote-ck-node::', 'action': 'shell', 'cmd': cmd, 'secretkey': secret_key})
        self.check(r)
        self.assertIn('stdout', r)
        self.assertIn('return_code', r)
        self.assertIn('stderr', r)
