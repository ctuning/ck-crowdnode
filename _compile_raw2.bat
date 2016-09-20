mkdir build
cd build

cl ../src/*.c /Feck-crowdtune.exe /link wsock32.lib
