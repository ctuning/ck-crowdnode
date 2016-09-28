mkdir build
cd build

rm ck-crowdnode-server 

cmake -DCMAKE_BUILD_TYPE=Debug ..

make

valgrind --tool=memcheck ./ck-crowdnode-server
