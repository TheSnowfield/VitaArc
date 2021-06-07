#!/bin/bash
cmake . -B build \
  && cd build    \
  && make clean  \
  && make -j 8
