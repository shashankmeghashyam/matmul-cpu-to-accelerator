#!/bin/bash
# =============================================================================
# run_all.sh
#
# Builds everything, runs the tests, times the real program, and runs the
# cache simulation. Everything printed is also saved to results.txt.
#
# How to run:   bash run_all.sh
# =============================================================================

set -e   # stop at the first error

{
echo "===== My CPU ====="
lscpu | grep "Model name"
lscpu -C
echo

echo "===== Step 1: Build ====="
g++ -std=c++17 -O2 -Wall test_cache.cpp     -o test_cache
g++ -std=c++17 -O2 -Wall matmul.cpp         -o matmul
g++ -std=c++17 -O2 -Wall simulate_matmul.cpp -o simulate_matmul
echo "Build OK"
echo

echo "===== Step 2: Tests (hand-checked answers) ====="
./test_cache
echo

echo "===== Step 3: Real timings on this laptop (1024 x 1024) ====="
./matmul naive 1024
./matmul interchange 1024
./matmul tiled 1024 32
echo

echo "===== Step 4: Cache simulation (512 x 512, this laptop's caches) ====="
./simulate_matmul naive 512
./simulate_matmul interchange 512
./simulate_matmul tiled 512 32
} 2>&1 | tee results.txt

echo "Done. Everything above was saved to results.txt"
