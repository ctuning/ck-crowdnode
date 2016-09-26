#!/bin/sh

set -x

build/ck-crowdnode-server >/dev/null &
pid=$!

die() {
    kill $pid
    exit $1
}

cd tests
rm -rf ck-master
unzip ck-master.zip
export PATH=$PWD/ck-master/bin:$PATH

ck add repo:remote-ck-node --url=http://localhost:3333 --remote --quiet

for SCRIPT in test*.sh
do
    if [ -f $SCRIPT -a -x $SCRIPT ]
    then
        ./$SCRIPT || die 1
    fi
done

kill $pid
rm -rf ck-master

set +x

exit 0
