#!/bin/bash

set -e

# you can skip "-G Ninja" and use "make" instead of "cmake --build build" for GNU experience
cmake -G Ninja -B build .
cmake --build build
