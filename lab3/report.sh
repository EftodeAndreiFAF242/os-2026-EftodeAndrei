#!/bin/bash
# usage: ./report.sh access.log
log=$1
if [ ! -f "$log" ]; then
  echo "no such file: $log" >&2
  exit 1
fi
total=$(wc -l < "$log")
fails=$(grep -c FAIL "$log")
echo "$total requests, $fails failed"
# failures of each user, from user0 to user6
for n in {0..6}; do
  echo "user$n: $(grep " user$n " "$log" | grep -c FAIL) failures"
done
