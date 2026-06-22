/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_oom.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 00:00:00 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/22 00:00:00 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/*
 * Out-of-memory / allocation-failure test for the LD_PRELOAD'd allocator.
 *
 * stat_mmap.c's accounting (__wrap_mmap/__wrap_munmap) and the RLIMIT_AS
 * ceiling cannot be reached directly: the version script makes those symbols
 * and g_malloc_state `local`, so a preloaded test binary cannot link against
 * them. This test therefore exercises the path *behaviorally*: it lowers
 * RLIMIT_AS, then checks that malloc fails *gracefully* (returns NULL, no crash
 * or abort) once the address-space ceiling is hit, and that freeing the blocks
 * lets allocation succeed again.
 *
 * It deliberately cannot isolate the library's own current_memory check from
 * the kernel's RLIMIT_AS enforcement — the kernel counts the whole process
 * address space (binary, libs, stacks, our mappings), so it generally trips
 * first. Either way this covers the failure-propagation path that the main
 * suite never touches: __wrap_mmap returns MAP_FAILED -> new_zone -> malloc_*
 * -> malloc() returns NULL.
 *
 * It is a separate target (make run_test_oom) because it perturbs a
 * process-wide limit and is mildly environment-sensitive; if the environment's
 * RLIMIT_AS is already too tight to leave headroom, the test SKIPs rather than
 * failing spuriously.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#define BLOCK (1u << 20)             /* 1 MB per allocation (LARGE class)     */
#define MAX_BLOCKS 512               /* hard cap so a broken limit can't run away */
#define HEADROOM (64UL * 1024 * 1024) /* address-space headroom above current */

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *name, int cond)
{
	if (cond) {
		printf("[PASS] %s\n", name);
		g_pass++;
	} else {
		printf("[FAIL] %s\n", name);
		g_fail++;
	}
}

/* Current virtual-memory size from /proc/self/statm, read without malloc. */
static size_t current_vm_bytes(void)
{
	char    buf[128];
	int     fd = open("/proc/self/statm", O_RDONLY);
	ssize_t n;
	size_t  pages = 0;
	int     i;

	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
		pages = pages * 10 + (size_t)(buf[i] - '0');
	return pages * (size_t)sysconf(_SC_PAGESIZE);
}

int main(void)
{
	struct rlimit rl;
	size_t        vm;
	size_t        target;
	static void  *blocks[MAX_BLOCKS];
	int           count    = 0;
	int           hit_null = 0;
	void         *recover;

	/* First print forces stdio to allocate its buffer while the address
	 * space is still unconstrained, so the result lines below print even
	 * once we are sitting against the ceiling. */
	printf("=== OOM / allocation-failure test ===\n");

	vm = current_vm_bytes();
	if (vm == 0 || getrlimit(RLIMIT_AS, &rl) != 0) {
		printf("[SKIP] cannot read current usage / RLIMIT_AS\n");
		return 0;
	}

	target = vm + HEADROOM;
	if (rl.rlim_max != RLIM_INFINITY && target > rl.rlim_max)
		target = rl.rlim_max;
	if (target <= vm + 2 * BLOCK) {
		printf("[SKIP] RLIMIT_AS headroom too small to test here\n");
		return 0;
	}

	rl.rlim_cur = target;
	if (setrlimit(RLIMIT_AS, &rl) != 0) {
		printf("[SKIP] setrlimit(RLIMIT_AS) failed\n");
		return 0;
	}

	/* Allocate large blocks until the address-space ceiling forces a NULL.
	 * (new_zone prints one "cannot create zone" line to stderr on the
	 * failing call — that is expected.) */
	while (count < MAX_BLOCKS) {
		void *p = malloc(BLOCK);

		if (p == NULL) {
			hit_null = 1;
			break;
		}
		((char *)p)[0]         = 1; /* touch both ends */
		((char *)p)[BLOCK - 1] = 1;
		blocks[count++]        = p;
	}

	check("some allocations succeed before the ceiling", count > 0);
	check("malloc returns NULL at the ceiling (no crash/abort)", hit_null);

	/* Free everything; the address space must come back. */
	while (count > 0)
		free(blocks[--count]);

	recover = malloc(64);
	check("allocation works again after freeing", recover != NULL);
	free(recover);

	printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
	return g_fail != 0;
}
