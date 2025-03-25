#!/bin/bash

PROJECT_ROOT=$(pwd)

rm -rf "$PROJECT_ROOT/build"

cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build" -G Ninja -DENABLE_OPENMP=ON
cmake --build "$PROJECT_ROOT/build"

$PROJECT_ROOT/build/main > out.txt
