mkdir build
cd build

rm ck-crowdnode-server 

cmake ..

make

valgrind --tool=memcheck ./ck-crowdnode-server
