
import filecmp
import unittest
import base64

# The following variables are initialized by test runner
ck=None                 # CK kernel
cfg=None                # test config
access_test_repo=None   # convenience function to call the test repo without the need to specify its UOA and secretkey.
                        # You just need to provide 'action' and the action's arguments

class TestPushPull(unittest.TestCase):

    def test_shell(self):
        cmd = 'echo test shell stdout'
        #different base64 because of OS specific line end
        stdoutBase64 = 'dGVzdCBzaGVsbCBzdGRvdXQgCg==' if 'Windows' == cfg['platform'] else 'dGVzdCBzaGVsbCBzdGRvdXQK'
        r = access_test_repo({'action': 'shell', 'cmd': cmd})
        self.assertIn('stdout_base64', r)
        self.assertEqual(stdoutBase64, r['stdout_base64'])
        self.assertIn('return_code', r)
        self.assertEqual(0, r['return_code'])
        self.assertIn('stderr_base64', r)
        self.assertEqual('', r['stderr_base64'])


    def test_shell_err(self):
        cmd = 'nodir C:\\' if 'Windows' == cfg['platform'] else 'nols -l /etc/'
        r = access_test_repo({'action': 'shell', 'cmd': cmd})
        self.assertIn('stdout_base64', r)
        self.assertEqual('', r['stdout_base64'])
        self.assertIn('return_code', r)
        self.assertNotEqual(0, r['return_code'])
        self.assertIn('stderr_base64', r)
        self.assertNotEqual('', r['stderr_base64'])


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

    def test_non_latin_encoding(self):
        fname = 'non-latin.txt.win' if 'Windows' == cfg['platform'] else 'non-latin.txt'
        fenc = '1252' if 'Windows' == cfg['platform'] else 'utf8'

        rl = ck.load_text_file({'text_file':fname, 'encoding':fenc})
        fcontent = rl['string']
        access_test_repo({'action': 'push', 'filename': fname})
        cmd = 'type ' + fname if 'Windows' == cfg['platform'] else 'cat ' + fname
        r = access_test_repo({'action': 'shell', 'cmd': cmd})

        xso=str(r.get('stdout_base64',''))
        enc=r.get('encoding','')
        so=''
        if xso!='':
            so=base64.urlsafe_b64decode(xso, )
            if type(so)==bytes:
                so=so.decode(encoding=enc, errors='ignore')
        try:
            self.assertEquals(fcontent, so)
        finally:
            try:
                os.remove(tmp_file)
            except: pass



