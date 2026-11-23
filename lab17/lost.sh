#!/bin/bash
# lost.sh - runs a program 20 times and measures the lost updates.
# Usage: ./lost.sh ./fire 2000000
prog=$1
expected=$2
total=0
min=$expected
max=0
for i in $(seq 20); do
    value=$($prog | grep '^counter:' | awk '{print $2}')
    lost=$((expected - value))
    total=$((total + lost))
    if [ "$lost" -lt "$min" ]; then min=$lost; fi
    if [ "$lost" -gt "$max" ]; then max=$lost; fi
done
average=$((total / 20))
percent=$((average * 100 / expected))
echo "$prog: expected $expected, lost on average $average ($percent%), least $min, most $max"
