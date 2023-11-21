#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
SDL_DIR=$ROOT_DIR/third_party/SDL

cd $SDL_DIR

cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DN3DS=1 -DSDL_AUDIO=1 -DSDL_FILESYSTEM=1 -DSDL_JOYSTICK=1 -DSDL_POWER=1 -DSDL_THREADS=1 -DSDL_TIMERS=1 -DSDL_SENSOR=1 -DSDL_VIDEO=1 -DSDL_LOCALE=1 -DSDL_FILE=1
cmake --build build
cmake --install build

cd -
