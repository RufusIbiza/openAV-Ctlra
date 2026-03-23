#!/bin/bash

echo "Building maschine_mk3_custom_graphics..."
gcc -o maschine_mk3_custom_graphics maschine_mk3_custom_graphics.c \
    -I../../ \
    -I../../ctlra \
    -L../../build/ctlra \
    -lctlra \
    $(pkg-config --cflags --libs cairo) \
    -lm -lusb-1.0 -ludev -lpthread

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "Run with: ./run_maschine_mk3.sh"
else
    echo "✗ Build failed!"
    exit 1
fi
