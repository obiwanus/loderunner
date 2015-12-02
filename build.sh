#!/bin/bash

echo "Starting build"

CFLAGS="-g -DBUILD_INTERNAL=1 -DBUILD_SLOW=1"

gcc $CFLAGS ../src/linux_loderunner.cpp $(pkg-config --cflags --libs x11) -o loderunner