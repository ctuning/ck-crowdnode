
import shutil
import os
import filecmp
import unittest

# The following variables are initialized by test runner
ck=None                 # CK kernel
cfg=None                # test config
access_test_repo=None   # convenience function to call the test repo without the need to specify its UOA and secretkey.
                        # You just need to provide 'action' and the action's arguments

class TestPushPull(unittest.TestCase):

    def test_push_pull(self):
        tmp_file = 'ck-push-test.zip'
        orig_file = 'ck-master.zip'
        shutil.copyfile(orig_file, tmp_file)
        try:
            access_test_repo({'action': 'push', 'filename': tmp_file})

            os.remove(tmp_file)

            access_test_repo({'action': 'pull', 'filename': tmp_file})

            # the downloaded file must match the original file
            self.assertTrue(filecmp.cmp(orig_file, tmp_file))
        finally:
            try:
                os.remove(tmp_file)
            except: pass

    def test_extra_path(self):
        tmp_file = 'ck-push-test.zip'
        orig_file = 'ck-master.zip'
        shutil.copyfile(orig_file, tmp_file)
        try:
            access_test_repo({'action': 'push', 'filename': tmp_file, 'extra_path': 'a'})

            os.remove(tmp_file)

            access_test_repo({'action': 'pull', 'filename': tmp_file, 'extra_path': 'a'})

            # the downloaded file must match the original file
            self.assertTrue(filecmp.cmp(orig_file, tmp_file))
        finally:
            try:
                os.remove(tmp_file)
            except: pass

    def test_extra_path_2(self):
        orig_file = 'ck-master.zip'
        r = access_test_repo({'action': 'push', 'filename': orig_file, 'extra_path': '1/2/3'}, checkFail=False)
        self.assertEqual(1, r['return'])

        # check two times to make sure the server is OK
        r = access_test_repo({'action': 'push', 'filename': orig_file, 'extra_path': '11/21/31'}, checkFail=False)
        self.assertEqual(1, r['return'])

        print(r)
