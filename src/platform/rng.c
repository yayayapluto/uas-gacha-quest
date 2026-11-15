#include "rng.h"

/* xorshift64*; 31-bit result mirrors the range rand() is used for (rand() % N). */
static unsigned long long state = 0x9E3779B97F4A7C15ULL;

void plat_srand(unsigned seed)
{
    state = ((unsigned long long)seed << 32) ^ seed;
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    if (state == 0) state = 0x9E3779B97F4A7C15ULL;
}

unsigned plat_rand(void)
{
    unsigned long long x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return (unsigned)((x * 0x2545F4914F6CDD1DULL) >> 33) & 0x7FFFFFFFu;
}

unsigned long long plat_rng_state(void) { return state; }

void plat_rng_set(unsigned long long s)
{
    state = s;
    if (state == 0) state = 0x9E3779B97F4A7C15ULL;
}