#!/bin/sh

# Required packages:
#   g++-arm-none-eabi
#   meson
#   ninja

# Commands for building the project
meson setup --cross-file ./compilation_setup.txt build --reconfig
cd ./build
ninja
