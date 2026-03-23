#!/bin/bash
# Run the D2 Custom Graphics program

cd "$(dirname "$0")"
export LD_LIBRARY_PATH=../../build/ctlra:$LD_LIBRARY_PATH
./d2_custom_graphics
