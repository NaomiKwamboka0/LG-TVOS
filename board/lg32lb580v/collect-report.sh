#!/bin/sh
# Plan v2.0 p.43. Run ON THE TV only after legitimate shell access exists (gate I02).
# Running it on a Mac/PC/VM describes that computer, not the TV.
# Prints a read-only report to stdout; creates no files.
set -f
show() {
  printf '\n[%s]\n' "$1"
  if [ -r "$1" ]; then
    cat "$1"
  else
    printf 'UNAVAILABLE\n'
  fi
}
printf 'TV-COMPAT-REPORT v1\n'
uname -srm
for p in /proc/cpuinfo /proc/meminfo /proc/partitions \
         /proc/mounts /proc/modules /proc/bus/input/devices \
         /proc/fb /proc/asound/cards \
         /sys/devices/system/cpu/online; do
  show "$p"
done
printf '\n[filesystem-capacity]\n'
df -k
printf '\n[end-of-report]\n'
