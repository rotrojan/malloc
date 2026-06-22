/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_mt.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/17 23:30:00 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/21 16:48:26 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/*
 * Multithreaded stress test for the arena allocator. Intended to be run with
 * the library injected via LD_PRELOAD, under helgrind:
 *
 *   LD_PRELOAD=./libft_malloc.so valgrind --tool=helgrind \
 *       --soname-synonyms=somalloc=NONE ./test_mt
 *
 * (--soname-synonyms=somalloc=NONE stops valgrind from replacing malloc with
 *  its own, so OUR allocator actually runs and helgrind can see its locking.)
 *
 * NT (16) > arena count (<=8) on purpose, so several threads share a home
 * arena -> exercises trylock-hop and per-arena contention. Phase B frees each
 * thread's blocks from a *different* thread, exercising the cross-thread
 * "go-home" free path. Sample-byte sentinels catch overlap/corruption; the
 * thread-safety verdict comes from helgrind.
 */

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NT 16 /* threads (> arena count to force sharing)            */
#define LOCAL_SLOTS 32 /* live blocks per thread in the churn phase */
#define LOCAL_ITERS 1200
#define PER 300 /* blocks per thread in the cross-thread phase         */

typedef struct block {
	void         *ptr;
	size_t        size;
	unsigned char tag;
} t_block;

/* Cross-thread phase: thread t fills its own PER-sized region; after the
 * barrier, a *different* thread frees it. Written before / read after the
 * barrier, so the array itself is race-free. */
static t_block           g_blocks[NT * PER];
static pthread_barrier_t g_barrier;

static size_t rand_size(unsigned int *seed)
{
	int r = rand_r(seed) % 100;

	if (r < 65)
		return 1 + rand_r(seed) % 128; /* TINY  */
	if (r < 95)
		return 129 + rand_r(seed) % (2032 - 129); /* SMALL */
	return 2033 + rand_r(seed) % 8192; /* LARGE */
}

/* Sample-byte sentinel: cheap under helgrind, still catches boundary overlap.
 */
static void mark(void *p, size_t n, unsigned char tag)
{
	unsigned char *b = p;

	b[0]     = tag;
	b[n / 2] = tag;
	b[n - 1] = tag;
}

static int verify(void *p, size_t n, unsigned char tag)
{
	unsigned char *b = p;

	return b[0] == tag && b[n / 2] == tag && b[n - 1] == tag;
}

static long churn(unsigned int *seed)
{
	t_block slots[LOCAL_SLOTS];
	long    fails = 0;

	for (int i = 0; i < LOCAL_SLOTS; i++)
		slots[i].ptr = NULL;

	for (int it = 0; it < LOCAL_ITERS; it++) {
		int s = rand_r(seed) % LOCAL_SLOTS;

		if (slots[s].ptr == NULL) {
			size_t        n = rand_size(seed);
			unsigned char t = (unsigned char)rand_r(seed);
			void         *p = malloc(n);

			if (p == NULL) {
				fails++;
				continue;
			}
			mark(p, n, t);
			slots[s] = (t_block){ p, n, t };
		} else if (!verify(slots[s].ptr, slots[s].size, slots[s].tag)) {
			fails++;
			slots[s].ptr = NULL; /* drop the corrupt slot */
		} else if (rand_r(seed) % 3 == 0) {
			free(slots[s].ptr);
			slots[s].ptr = NULL;
		} else {
			size_t        n  = rand_size(seed);
			void         *np = realloc(slots[s].ptr, n);
			unsigned char t;

			if (np == NULL) {
				fails++;
				continue;
			}
			if (((unsigned char *)np)[0] != slots[s].tag)
				fails++; /* realloc must preserve byte 0 */
			t = (unsigned char)rand_r(seed);
			mark(np, n, t);
			slots[s] = (t_block){ np, n, t };
		}
	}

	for (int i = 0; i < LOCAL_SLOTS; i++) {
		if (slots[i].ptr == NULL)
			continue;
		if (!verify(slots[i].ptr, slots[i].size, slots[i].tag))
			fails++;
		free(slots[i].ptr);
	}
	return fails;
}

static void *worker(void *arg)
{
	int          id    = (int)(intptr_t)arg;
	unsigned int seed  = (unsigned int)(id * 2654435761u + 1u);
	long         fails = 0;

	/* Phase A: concurrent local churn. */
	fails += churn(&seed);

	/* Phase B: allocate our own region... */
	for (int i = 0; i < PER; i++) {
		int           idx = id * PER + i;
		size_t        n   = rand_size(&seed);
		unsigned char t   = (unsigned char)(idx * 31 + 7);
		void         *p   = malloc(n);

		if (p == NULL) {
			fails++;
			g_blocks[idx].ptr = NULL;
			continue;
		}
		mark(p, n, t);
		g_blocks[idx] = (t_block){ p, n, t };
	}

	pthread_barrier_wait(&g_barrier);

	/* ...then free the NEXT thread's region (cross-thread / go-home free).
	 */
	int victim = (id + 1) % NT;
	for (int i = 0; i < PER; i++) {
		t_block *b = &g_blocks[victim * PER + i];

		if (b->ptr == NULL)
			continue;
		if (!verify(b->ptr, b->size, b->tag))
			fails++;
		free(b->ptr);
	}

	return (void *)(intptr_t)fails;
}

int main(void)
{
	pthread_t threads[NT];
	long      total_fails = 0;

	if (pthread_barrier_init(&g_barrier, NULL, NT) != 0) {
		fprintf(stderr, "barrier init failed\n");
		return 2;
	}

	for (int i = 0; i < NT; i++) {
		if (pthread_create(&threads[i], NULL, worker,
				   (void *)(intptr_t)i) != 0) {
			fprintf(stderr, "pthread_create failed\n");
			return 2;
		}
	}

	for (int i = 0; i < NT; i++) {
		void *ret;

		pthread_join(threads[i], &ret);
		total_fails += (long)(intptr_t)ret;
	}

	pthread_barrier_destroy(&g_barrier);

	if (total_fails != 0) {
		fprintf(stderr, "MT stress: %ld integrity failures\n",
			total_fails);
		return 1;
	}
	fprintf(stderr, "MT stress: OK (%d threads, no integrity failures)\n",
		NT);
	return 0;
}
