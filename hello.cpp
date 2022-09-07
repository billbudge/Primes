#include <iostream>
#include <cmath>
#include <chrono>

#include <vector>

using namespace std;

struct Entry {
    static const uint32_t BLOCK_SIZE = 10;
    uint16_t p;
    uint32_t offset;
};

// Array of already sieved blocks.
struct Block {
    Entry* entries;
    uint32_t count;
};
static std::vector<Block> g_sieved_blocks = std::vector<Block>();

// Use the global sieve for builing entries for the primes.
union SieveEntry {
    uint32_t number;
    Entry entry;
};
static SieveEntry* g_sieve1 = new SieveEntry[Entry::BLOCK_SIZE];
static SieveEntry* g_sieve2 = new SieveEntry[Entry::BLOCK_SIZE];

// TODO convert p to relative offset in Entry.
// TODO convert offset in entry to an offset mod block size. Requires mod check before sieving block.
void InitSieve(uint32_t index, SieveEntry* sieve) {
    uint32_t base = index * Entry::BLOCK_SIZE;
    for (int i = 0; i < Entry::BLOCK_SIZE; i++) {
        sieve[i].number = base + i;
    }
}

// Eliminate all multiples of prime in the sieve, starting at the given offset.
uint32_t SieveMultiples(uint32_t prime, uint32_t offset, SieveEntry* lowSieve) {
    uint32_t j = offset;
    for (; j < Entry::BLOCK_SIZE; j += prime) {
        lowSieve[j].number = 0;
    }
    uint32_t end_offset = j - Entry::BLOCK_SIZE;
    return end_offset;
}

// Eliminate the next multiple of prime in the sieve. If offset is greater than the segment size,
// remove it from the high sieve.
uint32_t SieveSingle(uint32_t prime, uint32_t offset, SieveEntry* lowSieve, SieveEntry* highSieve) {
    uint32_t end_offset = (offset + prime) % (2 * Entry::BLOCK_SIZE);
    if (offset < Entry::BLOCK_SIZE) {
        lowSieve[offset].number = 0;
        if (end_offset < offset)
            end_offset += Entry::BLOCK_SIZE;
    } else {
        highSieve[offset - Entry::BLOCK_SIZE].number = 0;
        if (end_offset > offset)
            end_offset -= Entry::BLOCK_SIZE;
    }
    // printf("erase: %i, prime: %i, offset: %i\n", highSieve[end_offset].number, prime, end_offset);
    return end_offset;
}

// Sieve and compact in a single pass for the first block.
uint32_t InitialSieve() {
    InitSieve(0, g_sieve1);
    InitSieve(1, g_sieve2);
    int num_primes = 0;
    for (int i = 2; i < Entry::BLOCK_SIZE; i++) {
        if (g_sieve1[i].number) {
            int prime = g_sieve1[i].number;
            int offset = SieveMultiples(prime, prime + prime, g_sieve1);
            Entry entry = { prime, offset };  // prime, and offset into next block for sieving
            g_sieve1[num_primes++].entry = entry;
        }
    }
    return num_primes;
}

// Move all found primes into entries at the beginning of the sieve. This will never overwrite any
// found primes.
uint32_t Compact(uint32_t index, SieveEntry* sieve) {
    uint32_t base = index * Entry::BLOCK_SIZE;
    uint32_t num_primes = 0;
    for (uint32_t i = 0; i < Entry::BLOCK_SIZE; i++) {
        if (sieve[i].number) {
            int prime = sieve[i].number;
            uint32_t p = prime;// - base;
            uint32_t offset = (prime + prime) % (2 * Entry::BLOCK_SIZE);
            Entry entry = { p, offset };
            sieve[num_primes++].entry = entry;
        }
    }
    return num_primes;
}

Block MakeBlock(uint32_t count, SieveEntry* sieve) {
    Entry* entries = new Entry[count];
    // copy the sieve entries into the right-sized block
    for (uint32_t i = 0; i < count; i++) entries[i] = sieve[i].entry;
    Block result = { entries, count };
    return result;
}

void SieveBlock0(Block block, uint32_t index, SieveEntry* lowSieve) {
    uint32_t base = index * Entry::BLOCK_SIZE;
    for (uint32_t i = 0; i < block.count; i++) {
        Entry &entry = block.entries[i];
        uint32_t offset = SieveMultiples(entry.p/* + base*/, entry.offset, lowSieve);
        entry.offset = offset;
    }
}

void SieveBlock(Block block, uint32_t index, SieveEntry* lowSieve, SieveEntry* highSieve) {
    uint32_t base = index * Entry::BLOCK_SIZE;
    for (uint32_t i = 0; i < block.count; i++) {
        Entry &entry = block.entries[i];
        uint32_t offset = SieveSingle(entry.p/* + base*/, entry.offset, lowSieve, highSieve);
        entry.offset = offset;
    }
}

void PrintSegment(Block block, uint32_t index) {
    printf("segment: %i  ", index);
    uint32_t base = index * Entry::BLOCK_SIZE;
    for (uint32_t i = 0; i < block.count; i++) {
        Entry &entry = block.entries[i];
        printf("%i, ", entry.p);// + base);
    }
    printf("\n");
}


int main() {
    // cout << "Hello, World!\n";
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    // Set up the first block
    uint32_t count = InitialSieve();
    uint32_t total = count;
    g_sieved_blocks.push_back(MakeBlock(count, g_sieve1));

    SieveEntry* lowSieve = g_sieve1;
    SieveEntry* highSieve = g_sieve2;

    for (uint32_t i = 1; i < 10; i++) {
        std::swap(lowSieve, highSieve);
                printf("sieving segment: %i, block: 0\n", i);
        SieveBlock0(g_sieved_blocks[0], 0, lowSieve);
        InitSieve(i + 1, highSieve);
        for (uint32_t j = 1; j < g_sieved_blocks.size(); j++) {
            if ((i / j) * j == i) {
                printf("sieving segment: %i, block: %i\n", i, j);
                if (j == 1) {
                    printf("here\n");
                }
                SieveBlock(g_sieved_blocks[j], j, lowSieve, highSieve);
            }
        }
        count = Compact(i, lowSieve);
        total += count;
        g_sieved_blocks.push_back(MakeBlock(count, lowSieve));
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    printf("microseconds: %i\n", (int)us);
    printf("count: %i\n", total);


    printf("\nsegments\n");
    for (uint32_t i = 0; i < g_sieved_blocks.size(); i++) {
        PrintSegment(g_sieved_blocks[i], i);
    }

    return 0;
}



// const int SIEVESIZE = 10000;
// int sieve[SIEVESIZE];

// bool isPrime(int n) {
//     int max = sqrt(n);
//     for (int i = 2; i < max; i++) {
//         if (i * i > n) return true;
//         if ((n / i) * i == n) return false;
//     }
//     return true;
// }

// int64_t primorial(int64_t n) {
//     int64_t result = 1;
//     int64_t j = 0;
//     for (int64_t i = 0; i < n; i++) {
//         while (sieve[j] == 0) j++;
//         result *= sieve[j];
//         j++;
//     }
//     return result;
// }

// int64_t gcd(int64_t a, int64_t b) {
//     while (a != b) {
//         if (a > b)
//             a = a - b;
//         else
//             b = b - a;
//     }
//     return a;
// }

    // for (int64_t i = 0; i < SIEVESIZE; i++) {
    //     sieve[i] = i;
    // }
    // int gap = 0;
    // int maxGap = 0;
    // for (int64_t i = 2; i < SIEVESIZE; i++) {
    //     if (sieve[i]) {
    //         int64_t prime = sieve[i];
    //         // printf("%i ", prime);
    //         for (int64_t j = prime + prime; j < SIEVESIZE; j += prime) {
    //             sieve[j] = 0;
    //         }
    //         printf("prime: %lli, gap: %i\n", prime, gap);
    //         if (gap > maxGap) maxGap = gap;
    //         gap = 0;
    //     } else {
    //         gap++;
    //     }
    // }

    // for (int64_t i = 1; i < 7; i++) {
    //     int64_t prim = primorial(i);
    //     int64_t relprime = 1;
    //     for (int64_t j = 2; j < prim; j++) {
    //         if (gcd(prim, j) == 1) {
    //             printf("relprime: %lli\n", j);
    //             relprime++;
    //         }
    //     }
    //     printf("primorial %lli: %lli, relprime: %lli, percent: %e\n", i, prim, relprime, 100 * (double)relprime / (double)prim);
    // }
