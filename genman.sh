#!/bin/bash

mkdir -p build && cd build && cmake .. && make -s && \
help2man -n "find memory leaks in applications" -N -o heapusage.1 ./heapusage
exit ${?}

