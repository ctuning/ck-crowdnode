
import filecmp
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
        self.assertIn('stdout_base64', r)
        self.assertIn('return_code', r)
        self.assertIn('stderr_base64', r)

    def test_shell_err(self):
        cmd = 'nodir C:\\' if 'Windows' == cfg['platform'] else 'nols -l /etc/'
        r = access_test_repo({'action': 'shell', 'cmd': cmd})
        self.assertIn('stdout_base64', r)
        self.assertIn('return_code', r)
        self.assertIn('stderr_base64', r)

    def test_non_latin(self):
        fname = 'non-latin.txt'
        access_test_repo({'action': 'push', 'filename': fname})
        cmd = 'type ' + fname if 'Windows' == cfg['platform'] else 'cat ' + fname

        r = ck.convert_file_to_upload_string({'filename': fname})
        base64_content = r['file_content_base64']

        r = access_test_repo({'action': 'shell', 'cmd': cmd})
        self.assertEqual(base64_content, r['stdout_base64'])

        r = ck.convert_upload_string_to_file({'file_content_base64': r['stdout_base64']})
        try:
            self.assertTrue(filecmp.cmp(fname, r['filename']))
        finally:
            try:
                os.remove(tmp_file)
            except: pass

