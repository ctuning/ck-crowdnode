mkdir build
cd build

rm * 

cmake -DCMAKE_BUILD_TYPE=Debug ..

make

valgrind --tool=memcheck ./ck-crowdnode-server
