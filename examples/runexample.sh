#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Usage: $0 <source_file.c>"
    exit 1
fi

FILE="$1"

mkdir -p build
gcc -I../include/ "$FILE" -lm -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -o build/exec

if [ $? -eq 0 ]; then
    cd build
    ./exec
else
    echo "Compilation failed."
fi
