// =============================================================================
// cache.h
//
// A simple model of a CPU cache.
//
// WHAT IS A CACHE?
//   Main memory (RAM) is slow. A cache is a small, fast memory inside the CPU
//   that keeps copies of recently used data. If the data the program needs is
//   already in the cache, that is a HIT (fast). If not, it is a MISS (slow),
//   and the data must be brought in from RAM.
//
// HOW THIS CACHE IS ORGANIZED
//   - Memory is moved in fixed-size chunks called BLOCKS (for example 64 bytes).
//   - The cache is divided into SETS. Each block of memory can only go into
//     one specific set.
//   - Each set has a few slots called WAYS. "4-way" means a set can hold
//     4 blocks at once.
//   - When a set is full and a new block arrives, one old block must be
//     thrown out. We throw out the LEAST RECENTLY USED (LRU) block.
// =============================================================================

#pragma once

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// One slot in the cache. It remembers which block of memory it is holding.
struct CacheLine {
    bool     isValid      = false;  // false = this slot is still empty
    uint64_t tag          = 0;      // identifies which memory block is stored here
    uint64_t lastUsedTime = 0;      // when it was last used (for LRU)
};

class Cache {
public:
    // Build a cache.
    //   totalSizeInBytes : the whole cache, e.g. 32768 for 32 KB
    //   blockSizeInBytes : size of one block, e.g. 64
    //   numberOfWays     : slots per set, e.g. 8 for an 8-way cache
    Cache(std::string cacheName,
          uint64_t totalSizeInBytes,
          uint64_t blockSizeInBytes,
          uint64_t numberOfWays)
    {
        name      = cacheName;
        blockSize = blockSizeInBytes;
        ways      = numberOfWays;

        uint64_t bytesPerSet = blockSize * ways;
        if (totalSizeInBytes % bytesPerSet != 0) {
            throw std::invalid_argument("Cache size must be a multiple of (block size x ways)");
        }
        numberOfSets = totalSizeInBytes / bytesPerSet;

        // Create all the sets. Each set starts with 'ways' empty slots.
        sets.resize(numberOfSets);
        for (uint64_t s = 0; s < numberOfSets; s++) {
            sets[s].resize(ways);
        }
    }

    // The program wants to read or write the byte at 'address'.
    // Returns true if it was a hit, false if it was a miss.
    bool access(uint64_t address)
    {
        currentTime = currentTime + 1;
        totalAccesses = totalAccesses + 1;

        // Step 1: Work out which memory block this address belongs to.
        //         Example with 64-byte blocks: addresses 0-63 are block 0,
        //         addresses 64-127 are block 1, and so on.
        uint64_t blockNumber = address / blockSize;

        // Step 2: Work out which set that block must go into.
        uint64_t setIndex = blockNumber % numberOfSets;

        // Step 3: The tag tells apart the different blocks that share a set.
        uint64_t tag = blockNumber / numberOfSets;

        std::vector<CacheLine>& set = sets[setIndex];

        // Step 4: Look through every slot in the set for our block.
        for (uint64_t w = 0; w < ways; w++) {
            if (set[w].isValid && set[w].tag == tag) {
                // HIT: the block is already in the cache.
                set[w].lastUsedTime = currentTime;
                totalHits = totalHits + 1;
                return true;
            }
        }

        // Step 5: MISS. Bring the block in, replacing a slot if needed.
        uint64_t slotToReplace = chooseSlotToReplace(set);
        set[slotToReplace].isValid      = true;
        set[slotToReplace].tag          = tag;
        set[slotToReplace].lastUsedTime = currentTime;
        totalMisses = totalMisses + 1;
        return false;
    }

    // Print a short summary of how the cache performed.
    void printSummary() const
    {
        std::cout << name << ": "
                  << totalAccesses << " accesses, "
                  << totalHits     << " hits, "
                  << totalMisses   << " misses, "
                  << "hit rate = " << hitRatePercent() << "%\n";
    }

    double hitRatePercent() const
    {
        if (totalAccesses == 0) {
            return 0.0;
        }
        return 100.0 * double(totalHits) / double(totalAccesses);
    }

    uint64_t getHits()     const { return totalHits; }
    uint64_t getMisses()   const { return totalMisses; }
    uint64_t getAccesses() const { return totalAccesses; }

private:
    // Pick which slot in a set to fill on a miss.
    //   - If there is an empty slot, use it.
    //   - Otherwise, replace the slot that was used longest ago (LRU).
    uint64_t chooseSlotToReplace(const std::vector<CacheLine>& set) const
    {
        for (uint64_t w = 0; w < ways; w++) {
            if (!set[w].isValid) {
                return w;  // found an empty slot
            }
        }

        uint64_t oldestSlot = 0;
        for (uint64_t w = 1; w < ways; w++) {
            if (set[w].lastUsedTime < set[oldestSlot].lastUsedTime) {
                oldestSlot = w;
            }
        }
        return oldestSlot;
    }

    std::string name;
    uint64_t blockSize    = 0;
    uint64_t ways         = 0;
    uint64_t numberOfSets = 0;

    std::vector<std::vector<CacheLine>> sets;  // sets[setIndex][way]

    uint64_t currentTime   = 0;  // counts up by 1 on every access
    uint64_t totalAccesses = 0;
    uint64_t totalHits     = 0;
    uint64_t totalMisses   = 0;
};
