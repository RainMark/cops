#!/bin/bash

CXX="/usr/local/gcc-10/bin/g++-10"
FLAGS="-g -std=c++17 -fno-sized-deallocation -D_GLIBCXX_USE_CXX11_ABI=0 -I."

mkdir -p bin

$CXX $FLAGS switch_test.cpp coroutine.cpp *.S -o bin/switch_test

$CXX $FLAGS future_test.cpp -o bin/future_test

$CXX $FLAGS -O0 loop_test.cpp coroutine.cpp *.S -o bin/loop_test

