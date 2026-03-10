#!/usr/bin/env bash

BUILD_TYPE="Debug"

cmake -B build -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
make -C build -j$(nproc)
