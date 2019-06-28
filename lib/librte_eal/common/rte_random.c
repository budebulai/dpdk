/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Ericsson AB
 */

#include <stdlib.h>

#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_random.h>

struct rte_rand_state {
	uint64_t z1;
	uint64_t z2;
	uint64_t z3;
	uint64_t z4;
	uint64_t z5;
} __rte_cache_aligned;

static struct rte_rand_state rand_states[RTE_MAX_LCORE];

static uint32_t
__rte_rand_lcg32(uint32_t *seed)
{
	*seed = 1103515245U * *seed + 12345U;

	return *seed;
}

static uint64_t
__rte_rand_lcg64(uint32_t *seed)
{
	uint64_t low;
	uint64_t high;

	/* A 64-bit LCG would have been much cleaner, but good
	 * multiplier/increments for such seem hard to come by.
	 */

	low = __rte_rand_lcg32(seed);
	high = __rte_rand_lcg32(seed);

	return low | (high << 32);
}

static uint64_t
__rte_rand_lfsr258_gen_seed(uint32_t *seed, uint64_t min_value)
{
	uint64_t res;

	res = __rte_rand_lcg64(seed);

	if (res < min_value)
		res += min_value;

	return res;
}

static void
__rte_srand_lfsr258(uint64_t seed, struct rte_rand_state *state)
{
	uint32_t lcg_seed;

	lcg_seed = (uint32_t)(seed ^ (seed >> 32));

	state->z1 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 2UL);
	state->z2 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 512UL);
	state->z3 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 4096UL);
	state->z4 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 131072UL);
	state->z5 = __rte_rand_lfsr258_gen_seed(&lcg_seed, 8388608UL);
}

void
rte_srand(uint64_t seed)
{
	unsigned int lcore_id;

	/* add lcore_id to seed to avoid having the same sequence */
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++)
		__rte_srand_lfsr258(seed + lcore_id, &rand_states[lcore_id]);
}

static __rte_always_inline uint64_t
__rte_rand_lfsr258_comp(uint64_t z, uint64_t a, uint64_t b, uint64_t c,
			uint64_t d)
{
	return ((z & c) << d) ^ (((z << a) ^ z) >> b);
}

/* Based on L’Ecuyer, P.: Tables of maximally equidistributed combined
 * LFSR generators.
 */

static __rte_always_inline uint64_t
__rte_rand_lfsr258(struct rte_rand_state *state)
{
	state->z1 = __rte_rand_lfsr258_comp(state->z1, 1UL, 53UL,
					    18446744073709551614UL, 10UL);
	state->z2 = __rte_rand_lfsr258_comp(state->z2, 24UL, 50UL,
					    18446744073709551104UL, 5UL);
	state->z3 = __rte_rand_lfsr258_comp(state->z3, 3UL, 23UL,
					    18446744073709547520UL, 29UL);
	state->z4 = __rte_rand_lfsr258_comp(state->z4, 5UL, 24UL,
					    18446744073709420544UL, 23UL);
	state->z5 = __rte_rand_lfsr258_comp(state->z5, 3UL, 33UL,
					    18446744073701163008UL, 8UL);

	return state->z1 ^ state->z2 ^ state->z3 ^ state->z4 ^ state->z5;
}

static __rte_always_inline
struct rte_rand_state *__rte_rand_get_state(void)
{
	unsigned int lcore_id;

	lcore_id = rte_lcore_id();

	if (unlikely(lcore_id == LCORE_ID_ANY))
		lcore_id = rte_get_master_lcore();

	return &rand_states[lcore_id];
}

uint64_t
rte_rand(void)
{
	struct rte_rand_state *state;

	state = __rte_rand_get_state();

	return __rte_rand_lfsr258(state);
}

RTE_INIT(rte_rand_init)
{
	rte_srand(rte_get_timer_cycles());
}