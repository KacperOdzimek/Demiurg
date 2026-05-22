#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Usage: $0 <source_file.c>"
    exit 1
fi

FILE="$1"

mkdir -p build
gcc -I../include/ -I/usr/include/libevdev-1.0 "$FILE" -lm -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -levdev -o build/exec

if [ $? -eq 0 ]; then
    cd build
    ./exec
else
    echo "Compilation failed."
fi
