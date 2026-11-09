// =============================================================================
// simulate_matmul.cpp
//
// Runs the same three matrix-multiply versions as matmul.cpp, but this time
// every memory read and write is ALSO sent to a model of my laptop's caches.
// At the end it reports how many hits and misses each cache level had.
//
// This answers the question: WHY is one version faster than another?
//
// The cache sizes below come from running `lscpu -C` on my laptop:
//   CPU: 11th Gen Intel Core i5-1135G7 ("Tiger Lake")
//
//   NAME  ONE-SIZE  WAYS  SETS  LINE SIZE
//   L1d     48K      12     64      64
//   L2     1.25M     20   1024      64
//   L3       8M       8  16384      64   (shared by all 4 cores)
//
// How to run:
//   ./simulate_matmul naive 512
//   ./simulate_matmul interchange 512
//   ./simulate_matmul tiled 512 32
// =============================================================================

#include "cache.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ---- My laptop's cache settings (from lscpu -C) ----
const uint64_t BLOCK_SIZE = 64;           // bytes per cache line
const uint64_t L1_SIZE    = 48 * 1024;    // 48 KB
const uint64_t L1_WAYS    = 12;
const uint64_t L2_SIZE    = 1280 * 1024;  // 1.25 MB
const uint64_t L2_WAYS    = 20;
const uint64_t L3_SIZE    = 8 * 1024 * 1024;  // 8 MB
const uint64_t L3_WAYS    = 8;

// -----------------------------------------------------------------------------
// A three-level memory system, like a real CPU:
//   - Every access goes to the small, fast L1 cache first.
//   - Only if L1 misses do we look in the bigger, slower L2 cache.
//   - Only if L2 misses do we look in the even bigger L3 cache.
//   - If L3 also misses, the data must come from RAM (the slowest step).
// -----------------------------------------------------------------------------
class MemorySystem {
public:
    MemorySystem()
        : l1("L1 data cache", L1_SIZE, BLOCK_SIZE, L1_WAYS),
          l2("L2 cache     ", L2_SIZE, BLOCK_SIZE, L2_WAYS),
          l3("L3 cache     ", L3_SIZE, BLOCK_SIZE, L3_WAYS)
    {
    }

    // Record one read or write of the number stored at 'location'.
    void access(const double* location)
    {
        // Turn the memory location of this number into a plain integer
        // address, exactly as the CPU sees it.
        uint64_t address = reinterpret_cast<uint64_t>(location);

        bool hitInL1 = l1.access(address);
        if (hitInL1) {
            return;
        }
        bool hitInL2 = l2.access(address);
        if (hitInL2) {
            return;
        }
        l3.access(address);
    }

    void printSummary() const
    {
        l1.printSummary();
        l2.printSummary();
        l3.printSummary();
        std::cout << "Trips to RAM (L3 misses): " << l3.getMisses() << "\n";
    }

private:
    Cache l1;
    Cache l2;
    Cache l3;
};

// Same helper as in matmul.cpp: (row, column) -> position in the list.
int position(int row, int column, int n)
{
    return row * n + column;
}

// -----------------------------------------------------------------------------
// Version 1: NAIVE (i, j, k). Reads B down a column -> big jumps in memory.
// -----------------------------------------------------------------------------
void simulateNaive(const std::vector<double>& A,
                   const std::vector<double>& B,
                   std::vector<double>& C,
                   int n,
                   MemorySystem& memory)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;  // kept in a CPU register, so no memory access
            for (int k = 0; k < n; k++) {
                memory.access(&A[position(i, k, n)]);   // read A
                memory.access(&B[position(k, j, n)]);   // read B
                sum = sum + A[position(i, k, n)] * B[position(k, j, n)];
            }
            memory.access(&C[position(i, j, n)]);       // write C
            C[position(i, j, n)] = sum;
        }
    }
}

// -----------------------------------------------------------------------------
// Version 2: LOOP INTERCHANGE (i, k, j). Reads B and C along rows -> in order.
// -----------------------------------------------------------------------------
void simulateInterchange(const std::vector<double>& A,
                         const std::vector<double>& B,
                         std::vector<double>& C,
                         int n,
                         MemorySystem& memory)
{
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < n; k++) {
            memory.access(&A[position(i, k, n)]);       // read A (once per k)
            double valueFromA = A[position(i, k, n)];
            for (int j = 0; j < n; j++) {
                memory.access(&C[position(i, j, n)]);   // read C
                memory.access(&B[position(k, j, n)]);   // read B
                memory.access(&C[position(i, j, n)]);   // write C
                C[position(i, j, n)] = C[position(i, j, n)] + valueFromA * B[position(k, j, n)];
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Version 3: TILED. Works on small tiles that stay inside the cache.
// -----------------------------------------------------------------------------
void simulateTiled(const std::vector<double>& A,
                   const std::vector<double>& B,
                   std::vector<double>& C,
                   int n,
                   int tileSize,
                   MemorySystem& memory)
{
    for (int rowStart = 0; rowStart < n; rowStart += tileSize) {
        for (int kStart = 0; kStart < n; kStart += tileSize) {
            for (int columnStart = 0; columnStart < n; columnStart += tileSize) {

                int rowEnd    = std::min(rowStart + tileSize, n);
                int kEnd      = std::min(kStart + tileSize, n);
                int columnEnd = std::min(columnStart + tileSize, n);

                for (int i = rowStart; i < rowEnd; i++) {
                    for (int k = kStart; k < kEnd; k++) {
                        memory.access(&A[position(i, k, n)]);       // read A
                        double valueFromA = A[position(i, k, n)];
                        for (int j = columnStart; j < columnEnd; j++) {
                            memory.access(&C[position(i, j, n)]);   // read C
                            memory.access(&B[position(k, j, n)]);   // read B
                            memory.access(&C[position(i, j, n)]);   // write C
                            C[position(i, j, n)] = C[position(i, j, n)] + valueFromA * B[position(k, j, n)];
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    std::string version = "naive";
    int n = 512;
    int tileSize = 32;

    if (argc > 1) { version  = argv[1]; }
    if (argc > 2) { n        = std::stoi(argv[2]); }
    if (argc > 3) { tileSize = std::stoi(argv[3]); }

    // Same starting values as matmul.cpp, so the checksums can be compared.
    std::vector<double> A(n * n);
    std::vector<double> B(n * n);
    std::vector<double> C(n * n, 0.0);
    for (int p = 0; p < n * n; p++) {
        A[p] = (p % 7) * 0.5;
        B[p] = (p % 5) * 0.25;
    }

    MemorySystem memory;

    if (version == "naive") {
        simulateNaive(A, B, C, n, memory);
    } else if (version == "interchange") {
        simulateInterchange(A, B, C, n, memory);
    } else if (version == "tiled") {
        simulateTiled(A, B, C, n, tileSize, memory);
    } else {
        std::cout << "Unknown version: " << version << "\n";
        std::cout << "Use: naive, interchange, or tiled\n";
        return 1;
    }

    double checksum = 0.0;
    for (double value : C) {
        checksum = checksum + value;
    }

    std::cout << "=== Simulated caches for: " << version
              << "   Size: " << n << " x " << n
              << "   Tile: " << tileSize << " ===\n";
    memory.printSummary();
    std::cout << "Checksum: " << std::fixed << checksum << "\n\n";
    return 0;
}
