#!/usr/bin/env bash
set -e

export VITASDK=/usr/local/vitasdk

cmake -S . -B build
cmake --build build --parallel 8
