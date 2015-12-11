#!/bin/bash

echo "Starting build"

CFLAGS="-g -std=c++11 -DBUILD_INTERNAL=1 -DBUILD_SLOW=1"

gcc $CFLAGS -shared -o libloderunner.so -fPIC ../src/loderunner.cpp
gcc $CFLAGS ../src/linux_loderunner.cpp $(pkg-config --cflags --libs x11) -o loderunner
