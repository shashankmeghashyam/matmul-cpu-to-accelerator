// =============================================================================
// matmul.cpp
//
// Multiplies two square matrices (C = A x B) in three different ways.
// All three do EXACTLY the same math and give the same answer.
// The only difference is the ORDER in which they read memory.
//
// Why that matters: the CPU cache loves reading memory in order
// (address 0, 1, 2, 3...). Jumping around in memory causes cache misses,
// and every miss makes the program wait for slow RAM.
//
// How to run:
//   ./matmul naive 1024
//   ./matmul interchange 1024
//   ./matmul tiled 1024 32        (the last number is the tile size)
// =============================================================================

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

// A matrix with n rows and n columns is stored as one long list of numbers,
// row after row ("row-major" order, the same way C and C++ store arrays):
//
//   row 0: positions 0   .. n-1
//   row 1: positions n   .. 2n-1
//   row 2: positions 2n  .. 3n-1   and so on.
//
// This helper turns (row, column) into a position in that list.
int position(int row, int column, int n)
{
    return row * n + column;
}

// -----------------------------------------------------------------------------
// Version 1: NAIVE (loop order i, j, k)
//
// This is how matrix multiply is usually taught. But the inner loop reads
// matrix B going DOWN a column. Moving down one row means jumping n numbers
// ahead in memory, so almost every read of B is a cache miss.
// -----------------------------------------------------------------------------
void multiplyNaive(const std::vector<double> &A,
                   const std::vector<double> &B,
                   std::vector<double> &C,
                   int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
            {
                sum = sum + A[position(i, k, n)] * B[position(k, j, n)];
            }
            C[position(i, j, n)] = sum;
        }
    }
}

// -----------------------------------------------------------------------------
// Version 2: LOOP INTERCHANGE (loop order i, k, j)
//
// Same loops, just swapped. Now the inner loop walks ALONG a row of B and C,
// one number after the next. Memory is read in order, so the cache works well.
// -----------------------------------------------------------------------------
void multiplyInterchange(const std::vector<double> &A,
                         const std::vector<double> &B,
                         std::vector<double> &C,
                         int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int k = 0; k < n; k++)
        {
            double valueFromA = A[position(i, k, n)];
            for (int j = 0; j < n; j++)
            {
                C[position(i, j, n)] = C[position(i, j, n)] + valueFromA * B[position(k, j, n)];
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Version 3: TILED (also called "blocked")
//
// Work on small square tiles (for example 32 x 32) instead of whole rows.
// A tile is small enough to stay inside the cache while we reuse it many
// times. This is the same idea that GPUs and Google's TPU chips use.
// -----------------------------------------------------------------------------
void multiplyTiled(const std::vector<double> &A,
                   const std::vector<double> &B,
                   std::vector<double> &C,
                   int n,
                   int tileSize)
{
    for (int rowStart = 0; rowStart < n; rowStart += tileSize)
    {
        for (int kStart = 0; kStart < n; kStart += tileSize)
        {
            for (int columnStart = 0; columnStart < n; columnStart += tileSize)
            {

                // The last tile may be smaller if n is not a multiple of tileSize.
                int rowEnd = std::min(rowStart + tileSize, n);
                int kEnd = std::min(kStart + tileSize, n);
                int columnEnd = std::min(columnStart + tileSize, n);

                // Multiply one tile, using the cache-friendly i, k, j order.
                for (int i = rowStart; i < rowEnd; i++)
                {
                    for (int k = kStart; k < kEnd; k++)
                    {
                        double valueFromA = A[position(i, k, n)];
                        for (int j = columnStart; j < columnEnd; j++)
                        {
                            C[position(i, j, n)] = C[position(i, j, n)] + valueFromA * B[position(k, j, n)];
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // ---- Read the command-line options (with sensible defaults) ----
    std::string version = "naive";
    int n = 512;
    int tileSize = 32;

    if (argc > 1)
    {
        version = argv[1];
    }
    if (argc > 2)
    {
        n = std::stoi(argv[2]);
    }
    if (argc > 3)
    {
        tileSize = std::stoi(argv[3]);
    }

    // ---- Fill A and B with simple, repeatable values. C starts at zero ----
    std::vector<double> A(n * n);
    std::vector<double> B(n * n);
    std::vector<double> C(n * n, 0.0);

    for (int p = 0; p < n * n; p++)
    {
        A[p] = (p % 7) * 0.5;
        B[p] = (p % 5) * 0.25;
    }

    // ---- Run the chosen version and time it ----
    auto startTime = std::chrono::steady_clock::now();

    if (version == "naive")
    {
        multiplyNaive(A, B, C, n);
    }
    else if (version == "interchange")
    {
        multiplyInterchange(A, B, C, n);
    }
    else if (version == "tiled")
    {
        multiplyTiled(A, B, C, n, tileSize);
    }
    else if (version == "setup-only")
    {
        // Do nothing: used to measure the cost of setup and checksum alone.
    }
    else
    {
        std::cout << "Unknown version: " << version << "\n";
                std::cout << "Use: naive, interchange, tiled, or setup-only\n";
        return 1;
    }

    auto endTime = std::chrono::steady_clock::now();
    double milliseconds = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // ---- Checksum: add up every number in C ----
    // All three versions must print the SAME checksum. That proves the faster
    // versions still compute the correct answer.
    double checksum = 0.0;
    for (double value : C)
    {
        checksum = checksum + value;
    }

    std::cout << "Version: " << version
              << "   Size: " << n << " x " << n
              << "   Tile: " << tileSize
              << "   Time: " << milliseconds << " ms"
              << "   Checksum: " << std::fixed << checksum << "\n";
    return 0;
}
