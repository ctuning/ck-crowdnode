#!/bin/sh

echo "\n============ Starting ck-crowdnode server\n"

build/ck-crowdnode-server &
pid=$!

die() {
    rm -rf ck-master
    kill $pid
    exit $1
}

cd test
rm -rf ck-master

sleep 1
echo "\n============ Downloading and initializing the latest CK\n"

# Clone the latest CK and initialize it
git clone https://github.com/ctuning/ck.git ck-master || die 1
export PATH=$PWD/ck-master/bin:$PATH
ck add repo:remote-ck-node --url=http://localhost:3333 --remote --quiet

retcode=0
for SCRIPT in test*.sh
do
    if [ -f $SCRIPT -a -x $SCRIPT ]
    then
        echo "\n============ Running $SCRIPT\n"
        ./$SCRIPT
        r=$?
        if [ $r -eq 0 ]; then
            echo "\n============ Test $SCRIPT: OK\n"
        else
            echo "\n============ Test $SCRIPT: Failed\n"
            retcode=$r
        fi
    fi
done

die $retcode
