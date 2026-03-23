#!/bin/bash

cd "$(dirname "$0")"
export LD_LIBRARY_PATH=../../build/ctlra:$LD_LIBRARY_PATH
./maschine_mk3_custom_graphics
