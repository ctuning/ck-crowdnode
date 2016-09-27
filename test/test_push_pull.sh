#!/bin/sh

# The test assumes 'remote-ck-node' repository is specified

tmp_file=ck-push-test.zip
cp ck-master.zip $tmp_file

die() {
    rm -f $tmp_file
    exit 1
}

ck push remote-ck-node:: --filename=$tmp_file || die

rm $tmp_file

ck pull remote-ck-node:: --filename=$tmp_file || die

diff $tmp_file ck-master.zip || die

rm -f $tmp_file

exit 0
