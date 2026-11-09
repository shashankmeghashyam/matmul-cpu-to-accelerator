#!/bin/bash
# =============================================================================
# validate.sh
#
# Checks my cache simulator against Valgrind Cachegrind, a widely used tool
# that measures cache misses of a real running program.
#
# Cachegrind is given the same cache sizes as my laptop (i5-1135G7):
#   L1 data: 48 KB, 12-way, 64-byte lines     -> --D1=49152,12,64
#   Last-level: 8 MB, 8-way, 64-byte lines    -> --LL=8388608,8,64
#
# "setup-only" runs everything EXCEPT the multiply. Its misses are the cost of
# creating the matrices and the checksum, which my simulator does not model.
# Subtracting it leaves the misses caused by the multiply alone.
#
# How to run:   bash validate.sh      (takes a few minutes)
# =============================================================================

CACHE_SETTINGS="--I1=32768,8,64 --D1=49152,12,64 --LL=8388608,8,64"

g++ -std=c++17 -O2 -Wall matmul.cpp -o matmul

for version in setup-only naive interchange tiled; do
    echo "===== Cachegrind: $version 512 ====="
    valgrind --tool=cachegrind --cache-sim=yes $CACHE_SETTINGS \
        --cachegrind-out-file=/dev/null ./matmul $version 512 2>&1 \
        | grep -E "D1 +misses|LLd +misses"
done | tee validation.txt

echo "Done. Saved to validation.txt"
