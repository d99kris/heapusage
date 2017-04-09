#!/bin/bash

cd src && help2man -n "find memory leaks and measure heap usage in applications" -N -o heapusage.1 ./heapusage
exit ${?}

