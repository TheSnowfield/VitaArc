#!/usr/bin/env bash
set -e

cmake -S . -B build
cmake --build build --parallel 8
