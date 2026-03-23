#!/bin/bash
# Build script for D2 Custom Graphics

echo "Building d2_custom_graphics..."
gcc -o d2_custom_graphics d2_custom_graphics.c \
    -I../../ \
    -I../../ctlra \
    -L../../build/ctlra \
    -lctlra \
    $(pkg-config --cflags --libs cairo sdl2) \
    -lm -lusb-1.0 -ludev -lpthread

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "Run with: ./run_d2_graphics.sh"
else
    echo "✗ Build failed!"
    exit 1
fi
