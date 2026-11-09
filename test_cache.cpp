// =============================================================================
// test_cache.cpp
//
// Small tests with answers worked out by hand on paper.
// If the simulator gives the same answers, we can trust it on big programs.
//
// How to read the expected answers:  H = hit,  M = miss
// =============================================================================

#include "cache.h"

#include <iostream>
#include <string>
#include <vector>

int numberOfFailedTests = 0;

// Send a list of addresses to the cache and compare the hits/misses
// with the answer we worked out by hand.
void runTest(std::string testName,
             Cache& cache,
             std::vector<uint64_t> addresses,
             std::string expectedResult)
{
    std::string actualResult = "";
    for (uint64_t address : addresses) {
        bool wasHit = cache.access(address);
        if (wasHit) {
            actualResult = actualResult + "H";
        } else {
            actualResult = actualResult + "M";
        }
    }

    if (actualResult == expectedResult) {
        std::cout << "PASS  " << testName << "   expected " << expectedResult
                  << ", got " << actualResult << "\n";
    } else {
        std::cout << "FAIL  " << testName << "   expected " << expectedResult
                  << ", got " << actualResult << "\n";
        numberOfFailedTests = numberOfFailedTests + 1;
    }
}

int main()
{
    // -------------------------------------------------------------------------
    // Test 1: Direct-mapped cache (1 way), 64 bytes total, 16-byte blocks
    //         -> 4 sets, each holding 1 block.
    //
    //   address 0  -> block 0 -> set 0   : MISS (cache is empty)
    //   address 4  -> block 0 -> set 0   : HIT  (same block as address 0)
    //   address 64 -> block 4 -> set 0   : MISS (and it kicks out block 0)
    //   address 0  -> block 0 -> set 0   : MISS (block 0 was kicked out)
    // -------------------------------------------------------------------------
    Cache directMapped("Test cache", 64, 16, 1);
    runTest("Direct-mapped conflict", directMapped, {0, 4, 64, 0}, "MHMM");

    // -------------------------------------------------------------------------
    // Test 2: 2-way cache, 64 bytes total, 16-byte blocks
    //         -> 2 sets, each holding 2 blocks.
    //         Addresses 0, 64 and 128 all map to set 0.
    //
    //   address 0   : MISS  set 0 = [0]
    //   address 64  : MISS  set 0 = [0, 64]
    //   address 0   : HIT   (0 is now the most recently used)
    //   address 128 : MISS  set is full, kick out the least recently used -> 64
    //   address 0   : HIT   0 is still there
    //   address 64  : MISS  64 was kicked out earlier
    // -------------------------------------------------------------------------
    Cache twoWay("Test cache", 64, 16, 2);
    runTest("2-way LRU replacement ", twoWay, {0, 64, 0, 128, 0, 64}, "MMHMHM");

    std::cout << "\n";
    if (numberOfFailedTests == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    } else {
        std::cout << numberOfFailedTests << " TEST(S) FAILED\n";
        return 1;
    }
}
