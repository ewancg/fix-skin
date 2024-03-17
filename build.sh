#!/usr/bin/env bash
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null
cmake -B build -S . -G Ninja
cmake --build build --config Release --parallel
