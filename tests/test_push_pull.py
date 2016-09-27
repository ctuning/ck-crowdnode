
import shutil
import os
import filecmp
import unittest

class TestPushPull(unittest.TestCase):

    def check(self, r):
        self.assertEqual(0, r['return'], str(r))

    def test_push_pull(self):
        tmp_file = 'ck-push-test.zip'
        orig_file = 'ck-master.zip'
        shutil.copyfile(orig_file, tmp_file)
        try:
            r = ck.access({'cid': 'remote-ck-node::', 'action': 'push', 'filename': tmp_file})
            self.check(r)

            os.remove(tmp_file)

            r = ck.access({'cid': 'remote-ck-node::', 'action': 'pull', 'filename': tmp_file})
            self.check(r)

            # the downloaded file must match the original file
            self.assertTrue(filecmp.cmp(orig_file, tmp_file))
        finally:
            os.remove(tmp_file)

