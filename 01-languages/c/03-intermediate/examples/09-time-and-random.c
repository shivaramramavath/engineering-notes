#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* Robust Xoroshiro64* pseudo-random generator (avoids rand() limitations) */
static uint64_t s_rng_state = 0x853c49e6748fea9bULL;

static inline uint32_t xoroshiro_next(void) {
    uint64_t x = s_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s_rng_state = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

/* Uniform integer distribution in range [min, max] without modulo bias */
static uint32_t uniform_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    uint32_t range = max - min + 1u;
    uint32_t threshold = (uint32_t)(0xFFFFFFFFu - (0xFFFFFFFFu % range));

    uint32_t r;
    do {
        r = xoroshiro_next();
    } while (r >= threshold);

    return min + (r % range);
}

int main(void) {
    printf("=== Time and Deterministic Random Generation ===\n");

    /* Time formatting */
    time_t now = time(NULL);
    struct tm *local_info = localtime(&now);
    char buf[128];
    if (local_info && strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC/Local", local_info)) {
        printf("Current formatted timestamp: %s\n", buf);
    }

    /* Benchmarking execution clock ticks */
    clock_t start_clock = clock();
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += (uint64_t)i;
    }
    clock_t end_clock = clock();
    double cpu_time_ms = ((double)(end_clock - start_clock) / CLOCKS_PER_SEC) * 1000.0;
    printf("Benchmarked 1M loop iterations: %.4f ms CPU time (accumulated: %llu)\n", cpu_time_ms, (unsigned long long)sum);

    /* Uniform unbiased random number generation */
    printf("Unbiased random rolls (1 to 6):\n  ");
    for (int i = 0; i < 10; i++) {
        printf("%u ", uniform_random_range(1, 6));
    }
    printf("\n");

    return 0;
}
