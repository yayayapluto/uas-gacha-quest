#ifndef RNG_H
#define RNG_H

/* Serializable PRNG: same call sites as rand() but the state can be saved and
   restored so a recovered session continues the exact same sequence. */
void plat_srand(unsigned seed);
unsigned plat_rand(void);
unsigned long long plat_rng_state(void);
void plat_rng_set(unsigned long long state);

#endif