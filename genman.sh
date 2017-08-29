#!/bin/bash

cd src && help2man -n "find memory leaks in applications" -N -o heapusage.1 ./heapusage
exit ${?}

