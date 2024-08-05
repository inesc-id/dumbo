#!/bin/bash

cat /proc/cpuinfo | grep "@.*GHz" | sed -n '1p' | sed -e 's/model.*@//g' | sed -e 's/GHz.*//g' > CPU_FREQ_GHZ.txt
echo "$(cat CPU_FREQ_GHZ.txt) * 1000000" | bc > CPU_FREQ_kHZ.txt
