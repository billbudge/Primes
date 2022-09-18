#include <iostream>
#include <cmath>
#include <chrono>

#include <vector>

using namespace std;

// An entry is created for each prime. Entries are stored in an array for each sieve segment which
// acts as a priority queue, sorted on offset. This allows us to avoid processing primes when they
// are irrelevant for the current sieve segment.
struct Entry {
    static const uint32_t BLOCK_SIZE = 65536;
    uint64_t p : 16, offset : 48;
};
    // Entry(uint16_t p, uint64_t offset) : p(p), offset(offset) {}
    // uint64_t p : 16, offset : 48;

// The primes computed from a segment of the sieve.
struct SegmentPrimes {
    Entry* entries;
    uint32_t count;
};
static std::vector<SegmentPrimes> g_sieved_primes = std::vector<SegmentPrimes>();

// Use the global sieve for finding the primes. Compact them into entries that are stored in the same array.
union SieveEntry {
    uint64_t number;
    Entry entry;
};
static SieveEntry* g_sieve = new SieveEntry[Entry::BLOCK_SIZE];

// TODO convert p to relative offset in Entry.
// TODO convert offset in entry to an offset mod segment size. Requires mod check before sieving segment.
void InitSieve(uint64_t index) {
    uint64_t base = index * Entry::BLOCK_SIZE;
    for (int i = 0; i < Entry::BLOCK_SIZE; i++) {
        g_sieve[i].number = base + i;
    }
}

// Eliminate all multiples of prime in the sieve, starting at the given offset.
uint64_t SieveMultiples(uint64_t prime, uint64_t offset) {
    uint64_t start = offset % Entry::BLOCK_SIZE;
    uint64_t j = start;
    for (; j < Entry::BLOCK_SIZE; j += prime) {
        // printf("erase prime: %i, offset: %i\n", prime, j);
        g_sieve[j].number = 0;
    }
    uint64_t end_offset = offset + j - start;
    return end_offset;
}

// Sieve and compact in a single pass for the first segment.
uint64_t InitialSieve() {
    InitSieve(0);
    int num_primes = 0;
    for (int i = 2; i < Entry::BLOCK_SIZE; i++) {
        if (g_sieve[i].number) {
            uint64_t prime = g_sieve[i].number;
            uint64_t offset = prime + prime;
            if (offset < Entry::BLOCK_SIZE) {
                offset = SieveMultiples(prime, offset);
            }
            // |prime| needs no adjustment for segment 0
            Entry entry = { static_cast<uint16_t>(prime), offset };
            g_sieve[num_primes++].entry = entry;
        }
    }
    return num_primes;
}

// Move all found primes into entries at the beginning of the sieve. This will never overwrite any
// found primes. Note that this array is sorted in increasing order on both prime and offset fields.
uint64_t Compact(uint64_t index) {
    uint64_t base = index * Entry::BLOCK_SIZE;
    uint64_t num_primes = 0;
    for (uint64_t i = 0; i < Entry::BLOCK_SIZE; i++) {
        if (g_sieve[i].number) {
            uint16_t prime = g_sieve[i].number;
            uint64_t offset = prime + prime;
            Entry entry = { prime - base, offset };
            g_sieve[num_primes++].entry = entry;
        }
    }
    return num_primes;
}

SegmentPrimes MakeSegmentPrimes(uint64_t count, uint64_t index) {
    Entry* entries = new Entry[count];
    // copy the sieve entries into the right-sized primes array.
    for (uint64_t i = 0; i < count; i++) {
        entries[i] = g_sieve[i].entry;
    }
    SegmentPrimes result = { entries, count };
    return result;
}

// Called after processing one sieve entry, and changing its offset.
// Sift down from the root to restore heap property.
void SiftDown(SegmentPrimes& primes) {
    Entry* entries = primes.entries;
    uint64_t parent = 0;
    while (true) {
        uint64_t first = parent * 2 + 1;
        if (first >= primes.count)
            break;
        uint64_t second = first + 1;
        uint64_t child = first;
        if (second < primes.count && entries[first].offset > entries[second].offset)
            child = second;
        if (entries[parent].offset < entries[child].offset)
            break;
        std::swap(entries[parent], entries[child]);
        parent = child;
    }
}

// When sieving segment 0, don't bother sorting on offset, as each entry must be processed
// for every new segment.
void SieveSegment0(SegmentPrimes& primes) {
    for (uint64_t i = 0; i < primes.count; i++) {
        Entry &entry = primes.entries[i];
        entry.offset = SieveMultiples(entry.p, entry.offset);
    }
}

void SieveSegment(SegmentPrimes& primes, uint64_t index, uint64_t endOffset) {
    uint64_t base = index * Entry::BLOCK_SIZE;
    while(true) {
        // Take the entry with the lowest offset.
        Entry &entry = primes.entries[0];
        if (entry.offset >= endOffset) break;
        entry.offset = SieveMultiples(entry.p + base, entry.offset);
        SiftDown(primes);
    }
}

void PrintSegment(SegmentPrimes& primes, uint64_t index) {
    printf("segment: %lli\n", index);
    uint64_t base = index * Entry::BLOCK_SIZE;
    for (uint64_t i = 0; i < primes.count; i++) {
        Entry &entry = primes.entries[i];
        printf("%lli, ", entry.p + base);
    }
    printf("\n");
}

bool IsPrime(uint64_t n) {
    for (uint64_t i = 2; i < n; i++) {
        if (i * i > n) return true;
        if ((n / i) * i == n) return false;
    }
    return true;
}

// (a * b) mod m
uint64_t MulModM(uint64_t a, uint64_t b, uint64_t m) {
    return ((a % m) * (b % m)) % m;
}
// Compute (a ^ d) mod m
uint64_t PowModM(uint64_t a, uint64_t d, uint64_t m) {
    if (m == 1) return 0;
    uint64_t result = 1;
    uint64_t base = a % m;
    while (d > 0) {
        if (d & 1)
            result = (result * base) % m;
        base = (base * base) % m;
        d = d >> 1;
    }
    return result;
}
// Input #1: n > 3, an odd integer to be tested for primality
// Input #2: k, the number of rounds of testing to perform
// Output: “composite” if n is found to be composite, “probably prime” otherwise

// write n as 2s^d + 1 with d odd (by factoring out powers of 2 from n − 1)
// WitnessLoop: repeat k times:
//     pick a random integer a in the range [2, n − 2]
//     x ← a^d mod n
//     if x = 1 or x = n − 1 then
//         continue WitnessLoop
//     repeat s − 1 times:
//         x ← x2 mod n
//         if x = n − 1 then
//             continue WitnessLoop
//     return “composite”
// return “probably prime”
bool IsEven(uint64_t n) { return (!(n & 1)); }
bool MillerRabinPrimalityTest(uint64_t n) {
    if (n < 1024)
        return IsPrime(n);
    assert (n > 2);
    if (IsEven(n)) return false;  // divisible by 2
    uint64_t s = 1;
    uint64_t d = (n - 1) / 2;
    while (!(d & 1)) {
        s++;
        d = d / 2;
    }
    // witness loop
    uint64_t bases[] = { 2, 325, 9375, 28178, 450775, 557157, 9780504, 1795265022 };
    for (int i = 0; i < 7; i++) {
        uint64_t witness = bases[i];
        uint64_t x = PowModM(witness, d, n);
        if (x == 1 || x == n - 1)
            continue;
        uint32_t j = 1;
        for (; j < s; j++) {
            x = MulModM(x, x, n);
            if (x == n - 1)
                break;
        }
        if (j == s) {
            // if (IsPrime(n)) {
            //     printf("error: %lli\n", n);
            // }
            return false;
        }
    }
    // assert(IsPrime(n));
    return true;
}





int main() {
    // uint64_t test = PowModM(4, 13, 497);
    // printf(" 4^13 (mod 497): %lli\n", test);

    bool test1 = IsPrime(1966079993663);
    bool test2 = MillerRabinPrimalityTest(1966079993663);
    printf("1966079993663 prime: %i, %i\n", test1, test2);
    {
        // // brute force primality testing
        // std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        // uint32_t count = 1;  // count 2
        // for (uint64_t i = 3; i < 65536 * 1000; i++) {
        //     if (IsPrime(i)) count++;
        // }
        // std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        // long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        // printf("microseconds: %i\n", (int)us);
        // printf("count: %i\n", count);
    }
    {
        // Miller-Rabin primality testing
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        uint32_t count = 1;
        for (uint64_t i = 3; i < 65536 * 10000; i++) {
            if (MillerRabinPrimalityTest(i)) count++;
        }
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        printf("microseconds: %i\n", (int)us);
        printf("count: %i\n", count);
    }
    {
        // Simple sieve.
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        const uint64_t SIEVESIZE = 65536 * 10000;
        uint32_t* sieve = new uint32_t[SIEVESIZE];
        for (uint64_t i = 0; i < SIEVESIZE; i++) {
            sieve[i] = i;
        }
        uint32_t count = 0;
        for (int64_t i = 2; i < SIEVESIZE; i++) {
            if (sieve[i]) {
                int64_t prime = sieve[i];
                count++;
                // printf("%i ", prime);
                for (int64_t j = prime + prime; j < SIEVESIZE; j += prime) {
                    sieve[j] = 0;
                }
            }
        }
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        printf("microseconds: %i\n", (int)us);
        printf("count: %i\n", count);
        delete[] sieve;
    }
    {
        // segmented sieve
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        // Set up the first segment and sieve to get the primes.
        uint32_t count = InitialSieve();
        g_sieved_primes.push_back(MakeSegmentPrimes(count, 0));

        uint64_t total = count;
        for (uint64_t i = 1; i < 1000; i++) {
            InitSieve(i);
            SieveSegment0(g_sieved_primes[0]);
            for (uint64_t j = 1; j < g_sieved_primes.size(); j++) {
                // printf("sieving segment: %i, from: %i\n", i, j);
                SieveSegment(g_sieved_primes[j], j, i * Entry::BLOCK_SIZE + Entry::BLOCK_SIZE);
            }
            count = Compact(i);
            g_sieved_primes.push_back(MakeSegmentPrimes(count, i));
            total += count;
        }

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        printf("microseconds: %i\n", (int)us);
        printf("count: %lli\n", total);

        // printf("\nsegments\n");
        // for (uint32_t i = 0; i < g_sieved_primes.size(); i++) {
        //     PrintSegment(g_sieved_primes[i], i);
        // }
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
