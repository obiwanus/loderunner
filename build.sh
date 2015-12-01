#!/bin/bash

echo "Starting build"

CFLAGS=""

gcc $CFLAGS ../src/linux_loderunner.cpp $(pkg-config --cflags --libs x11) -o loderunner