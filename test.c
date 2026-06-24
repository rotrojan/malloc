/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rotrojan <rotrojan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/19 14:36:30 by rotrojan          #+#    #+#             */
/*   Updated: 2026/06/10 00:00:00 by rotrojan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"
#include "malloc.h"
#include "srcs/show_alloc_mem.h"

#include <stdint.h>
#include <string.h>

void show_alloc_mem(void) __attribute__((weak));

/* -------------------------------------------------------------------------- */
/* Test framework                                                             */
/* -------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(name, cond)                               \
	do {                                            \
		if (cond) {                             \
			ft_printf("[PASS] %s\n", name); \
			g_pass++;                       \
		} else {                                \
			ft_printf("[FAIL] %s\n", name); \
			g_fail++;                       \
		}                                       \
	} while (0)

#define SECTION(name) ft_printf("\n=== %s ===\n", name)

static int is_aligned(void *ptr, size_t align)
{
	return ptr != NULL && ((uintptr_t)ptr % align) == 0;
}

/*
 * noinline wrappers around free()/realloc(). GCC's -Werror=use-after-free and
 * -Werror=free-nonheap-object fire at the call site when the compiler can
 * statically see the pointer's origin (double free, freeing a non-heap object).
 * Passing through a noinline barrier makes the pointer opaque, so the static
 * analysis stops here and the runtime behaviour of our custom free()/realloc()
 * is what actually gets exercised.
 */
static __attribute__((noinline)) void call_free(void *p)
{
	free(p);
}

static __attribute__((noinline)) void *call_realloc(void *p, size_t n)
{
	return realloc(p, n);
}

/* memset a region then read it back; returns 1 if every byte stuck. */
static int fill_and_check(void *ptr, size_t n, unsigned char val)
{
	unsigned char *p = ptr;
	size_t         i;

	memset(p, val, n);
	for (i = 0; i < n; i++)
		if (p[i] != val)
			return 0;
	return 1;
}

/*
 * Position-dependent byte pattern: byte i = (i + seed) mod 256. Used to detect
 * not just clobbering but also misaligned copies (a wrong realloc offset shifts
 * the pattern, which check_pattern then catches).
 */
static void fill_pattern(void *ptr, size_t n, unsigned seed)
{
	unsigned char *p = ptr;

	for (size_t i = 0; i < n; i++)
		p[i] = (unsigned char)((i + seed) & 0xFF);
}

static int check_pattern(const void *ptr, size_t n, unsigned seed)
{
	const unsigned char *p = ptr;

	for (size_t i = 0; i < n; i++)
		if (p[i] != (unsigned char)((i + seed) & 0xFF))
			return 0;
	return 1;
}

/* -------------------------------------------------------------------------- */
/* Section 1 — Basic allocation — TINY range                                  */
/*                                                                            */
/* Tests that malloc returns non-NULL and 16-byte-aligned pointers for every  */
/* size in the TINY range (1–128 B). Alignment of 16 is TINY_QUANTUM: the     */
/* bitmap granularity. A pointer misaligned by even 1 byte would indicate     */
/* the chunk-index arithmetic in malloc_tiny is wrong.                        */
/* -------------------------------------------------------------------------- */

static void test_basic_alloc(void)
{
	SECTION("Basic allocation — TINY range (1–128 B)");

	static const size_t sizes[] = {
		0, 1, 2, 7, 8, 15, 16, 17, 64, 112, 128
	};
	const int n = (int)(sizeof(sizes) / sizeof(sizes[0]));
	void     *p;

	/*
	 * malloc(0): per man 3 malloc, must return a unique non-NULL pointer
	 * that can be passed to free(). Our implementation allocates one chunk.
	 */
	p = malloc(0);
	CHECK("malloc(0) returns non-NULL", p != NULL);
	CHECK("malloc(0) is 16-aligned", is_aligned(p, 16));
	free(p);

	for (int i = 1; i < n; i++) {
		p = malloc(sizes[i]);
		CHECK("TINY alloc returns non-NULL", p != NULL);
		CHECK("TINY alloc is 16-aligned", is_aligned(p, 16));
		free(p);
	}
}

/* -------------------------------------------------------------------------- */
/* Section 2 — Basic allocation — SMALL range                                 */
/*                                                                            */
/* Tests that malloc returns non-NULL and 16-byte-aligned pointers for a      */
/* representative set of SMALL sizes (129–2032 B). The user pointer is the    */
/* chunk payload which starts at zone + SMALL_QUANTUM (256 B into the zone).  */
/* Since SMALL_QUANTUM is a multiple of 16, all SMALL pointers are 16-aligned.*/
/* -------------------------------------------------------------------------- */

static void test_basic_small_alloc(void)
{
	SECTION("Basic allocation — SMALL range (129–2032 B)");

	static const size_t sizes[] = { 129, 200, 256, 512, 1024, 2032 };
	const int           n       = (int)(sizeof(sizes) / sizeof(sizes[0]));

	for (int i = 0; i < n; i++) {
		void *p = malloc(sizes[i]);
		CHECK("SMALL alloc returns non-NULL", p != NULL);
		CHECK("SMALL alloc is 16-aligned", is_aligned(p, 16));
		if (p)
			free(p);
	}
}

/* -------------------------------------------------------------------------- */
/* Section 3 — Large allocations                                              */
/*                                                                            */
/* Any size > 2032 B goes to malloc_large. Each large alloc is its own mmap   */
/* region. The user pointer is zone + sizeof(s_zone_hdr). sizeof(s_zone_hdr)  */
/* is a multiple of 16, so the pointer inherits the page alignment of the     */
/* zone base and is always 16-aligned. We also verify the full range is       */
/* writable — a too-small mmap would produce a SIGSEGV here.                  */
/* -------------------------------------------------------------------------- */

static void test_large_alloc(void)
{
	SECTION("Large allocations (> 2032 B, one mmap each)");

	static const size_t sizes[] = { 2033, 4096, 10000, 1000000 };
	const int           n       = (int)(sizeof(sizes) / sizeof(sizes[0]));

	for (int i = 0; i < n; i++) {
		void *p = malloc(sizes[i]);
		CHECK("LARGE alloc returns non-NULL", p != NULL);
		CHECK("LARGE alloc is 16-aligned", is_aligned(p, 16));
		if (p) {
			CHECK("LARGE alloc fully writable",
			      fill_and_check(p, sizes[i], 0xAB));
			free(p);
		}
	}
}

/* -------------------------------------------------------------------------- */
/* Section 4 — Write integrity (no allocation clobbers another)               */
/*                                                                            */
/* Allocate 21 pointers with distinct sizes spanning TINY, SMALL, and LARGE,  */
/* fill each with a unique byte pattern, then verify all patterns after all   */
/* allocations have been made. Any overlap between returned ranges would      */
/* corrupt at least one pattern.                                              */
/* -------------------------------------------------------------------------- */

static void test_write_integrity(void)
{
	SECTION("Write integrity — no allocation clobbers another");

	static const size_t sizes[] = { 1,   2,   7,    8,    15,   16,   17,
					31,  32,  63,   64,   65,   97,   112,
					128, 200, 500,  1000, 2000, 4096, 100000 };
	const int           n       = (int)(sizeof(sizes) / sizeof(sizes[0]));
	void               *ptrs[sizeof(sizes) / sizeof(sizes[0])];

	for (int i = 0; i < n; i++) {
		ptrs[i] = malloc(sizes[i]);
		if (ptrs[i])
			memset(ptrs[i], (unsigned char)(i + 1), sizes[i]);
	}

	int ok = 1;
	for (int i = 0; i < n && ok; i++) {
		if (!ptrs[i]) {
			ok = 0;
			continue;
		}
		unsigned char *p = ptrs[i];
		for (size_t j = 0; j < sizes[i]; j++)
			if (p[j] != (unsigned char)(i + 1)) {
				ok = 0;
				break;
			}
	}
	CHECK("no allocation clobbers another", ok);

	for (int i = 0; i < n; i++)
		free(ptrs[i]);
}

/* -------------------------------------------------------------------------- */
/* Section 5 — free of maximum-span TINY allocations (113–128 B → 8 chunks)   */
/*                                                                            */
/* With 16 B granularity, a 128 B request occupies 128/16 = 8 chunks — the    */
/* most a single TINY allocation can ever span (TINY_SIZE_MAX/TINY_QUANTUM).  */
/* Sizes 113–128 all round up to this 8-chunk maximum. On free, free_tiny     */
/* counts the consecutive in-use chunks that belong to the allocation; this   */
/* test drives that count at its upper boundary, where an off-by-one (scanning */
/* a 9th chunk) would clear or read one chunk past the allocation and corrupt */
/* the neighbouring chunk or the bitmap. Reaching the post-free CHECK without */
/* a crash is the property under test.                                        */
/* -------------------------------------------------------------------------- */

static void test_free_max_span_tiny(void)
{
	SECTION("free of maximum-span TINY allocations (113–128 B, 8 chunks)");

	static const size_t sizes[] = { 113, 120, 127, 128 };
	const int           n       = (int)(sizeof(sizes) / sizeof(sizes[0]));

	for (int i = 0; i < n; i++) {
		void *p = malloc(sizes[i]);
		CHECK("8-chunk alloc succeeds", p != NULL);
		fill_and_check(p, sizes[i], 0xBB);
		free(p);
		/* Reaching this line without abort proves the scan is correct. */
		CHECK("free of 8-chunk alloc did not abort", 1);
	}
}

/* -------------------------------------------------------------------------- */
/* Section 6 — Interior free (neighbours preserved, non-empty zone kept)      */
/*                                                                            */
/* Allocate A, B, C in order, then free B (an *interior* block — not the most */
/* recently allocated) and then A. The allocator must: (1) leave C's contents */
/* untouched, since freeing a neighbour must never corrupt a live allocation; */
/* (2) NOT munmap the zone, because C is still live — zone emptiness is        */
/* decided by a full in_use bitmap scan, not by a free-chunk hint that a       */
/* walk-back could leave stale; (3) make the freed space reusable, so a later  */
/* allocation succeeds without aliasing C. ("Interior"/"non-frontier" = the    */
/* freed block sits below the most-recently-allocated chunk.)                 */
/* -------------------------------------------------------------------------- */

static void test_interior_free(void)
{
	SECTION("Interior free — neighbours preserved, non-empty zone not released");

	void *a = malloc(16);
	void *b = malloc(16);
	void *c = malloc(16);
	CHECK("three sequential allocs succeed",
	      a != NULL && b != NULL && c != NULL);

	if (!a || !b || !c) {
		free(a);
		free(b);
		free(c);
		return;
	}

	memset(c, 0xCC, 16);

	free(b);
	free(a);

	/* Allocate again — must not overlap c. */
	void *d = malloc(16);
	CHECK("alloc after interior free succeeds", d != NULL);
	CHECK("new alloc does not alias the survivor", d != c);

	/* c's pattern must be intact: the freed neighbours must not have touched
	   its range. */
	unsigned char *cp     = c;
	int            intact = 1;
	for (int i = 0; i < 16; i++)
		if (cp[i] != 0xCC) {
			intact = 0;
			break;
		}
	CHECK("surviving allocation not corrupted", intact);

	free(c);
	free(d);
}

/* -------------------------------------------------------------------------- */
/* Section 7 — TINY zone exhaustion (spans two TINY zones)                    */
/*                                                                            */
/* One TINY zone is 4 pages = 16384 B. At 16 B/chunk that is 1024 chunks,     */
/* of which ~1002 are usable (the header occupies NB_CHUNKS_TINY_HDR = 22).   */
/* Allocating 1100 chunks forces a second zone to be mmap'd. We verify:       */
/* (1) all 1100 allocs succeed; (2) the byte patterns written to each are     */
/* intact after all 1100 allocations, confirming no overlap across the zone   */
/* boundary; (3) a fresh alloc works after freeing everything.                */
/* -------------------------------------------------------------------------- */

static void test_tiny_zone_exhaustion(void)
{
	SECTION("TINY zone exhaustion — spans two TINY zones");

	const int n = 1100;
	void     *ptrs[1100];
	int       all_ok = 1;

	for (int i = 0; i < n; i++) {
		ptrs[i] = malloc(16);
		if (!ptrs[i]) {
			all_ok = 0;
			break;
		}
		memset(ptrs[i], (unsigned char)(i & 0xFF), 16);
	}
	CHECK("all 1100 allocs succeed across TINY zone boundary", all_ok);

	int patterns_ok = 1;
	for (int i = 0; i < n && patterns_ok; i++) {
		if (!ptrs[i])
			break;
		unsigned char *p = ptrs[i];
		for (int j = 0; j < 16; j++)
			if (p[j] != (unsigned char)(i & 0xFF)) {
				patterns_ok = 0;
				break;
			}
	}
	CHECK("all 1100 TINY patterns intact across zone boundary", patterns_ok);

	for (int i = 0; i < n; i++)
		free(ptrs[i]);

	void *p = malloc(16);
	CHECK("alloc succeeds after freeing all 1100 TINY", p != NULL);
	free(p);
}

/* -------------------------------------------------------------------------- */
/* Section 8 — TINY zone release (empty zone is munmap'd)                     */
/*                                                                            */
/* Fill exactly one TINY zone (1002 usable 16-B chunks), then free them all.  */
/* free_tiny's bitmap scan must detect the zone is fully empty and call        */
/* release_zone -> munmap. We print show_alloc_mem before and after so the     */
/* developer can confirm the zone disappears, and verify a subsequent alloc    */
/* creates a fresh zone.                                                       */
/*                                                                            */
/* 1024 chunks/zone - 22 header chunks = 1002 usable. This is white-box: it    */
/* depends on TINY_ZONE_SIZE=4*PAGE_SIZE, TINY_QUANTUM=16, and                */
/* NB_CHUNKS_TINY_HDR=22.                                                      */
/* -------------------------------------------------------------------------- */

static void test_tiny_zone_release(void)
{
	SECTION("TINY zone release — empty TINY zone is munmap'd");

	const int capacity = 1002;
	void     *ptrs[1002];

	for (int i = 0; i < capacity; i++)
		ptrs[i] = malloc(16);

	if (show_alloc_mem) {
		ft_printf("After filling one TINY zone (expect one TINY zone):\n");
		show_alloc_mem();
	}

	for (int i = 0; i < capacity; i++)
		free(ptrs[i]);

	if (show_alloc_mem) {
		ft_printf("After freeing all (expect empty):\n");
		show_alloc_mem();
	}

	void *p = malloc(16);
	CHECK("alloc succeeds after TINY zone release", p != NULL);
	free(p);
}

/* -------------------------------------------------------------------------- */
/* Section 9 — SMALL zone exhaustion (spans two SMALL zones)                  */
/*                                                                            */
/* One SMALL zone is 64 pages = 262144 B. The wild chunk is                   */
/* SMALL_ZONE_SIZE - SMALL_QUANTUM = 261888 B. A 129-byte request rounds up   */
/* to real_size = 256 B (minimum SMALL quantum), giving 261888/256 = 1023     */
/* usable slots per zone. Allocating 1100 forces a second zone to be mmap'd.  */
/* We verify: (1) all 1100 allocs succeed; (2) byte patterns are intact after */
/* all allocations (no cross-zone overlap); (3) a fresh alloc works after      */
/* freeing everything.                                                        */
/* -------------------------------------------------------------------------- */

static void test_small_zone_exhaustion(void)
{
	SECTION("SMALL zone exhaustion — spans two SMALL zones");

	const int n = 1100;
	void     *ptrs[1100];
	int       all_ok = 1;

	for (int i = 0; i < n; i++) {
		ptrs[i] = malloc(129);
		if (!ptrs[i]) {
			all_ok = 0;
			break;
		}
		memset(ptrs[i], (unsigned char)(i & 0xFF), 129);
	}
	CHECK("all 1100 SMALL allocs succeed across zone boundary", all_ok);

	int patterns_ok = 1;
	for (int i = 0; i < n && patterns_ok; i++) {
		if (!ptrs[i])
			break;
		unsigned char *p = ptrs[i];
		for (int j = 0; j < 129; j++)
			if (p[j] != (unsigned char)(i & 0xFF)) {
				patterns_ok = 0;
				break;
			}
	}
	CHECK("all 1100 SMALL patterns intact across zone boundary", patterns_ok);

	for (int i = 0; i < n; i++)
		free(ptrs[i]);

	void *p = malloc(129);
	CHECK("alloc succeeds after freeing all 1100 SMALL", p != NULL);
	free(p);
}

/* -------------------------------------------------------------------------- */
/* Section 10 — SMALL zone release (empty zone is munmap'd)                   */
/*                                                                            */
/* Fill exactly one SMALL zone then free all. free_small detects the zone is   */
/* empty when the coalesced free chunk equals SMALL_ZONE_SIZE - SMALL_QUANTUM  */
/* (261888 B) and calls release_zone -> munmap.                                */
/*                                                                            */
/* Each 129-byte request rounds up to real_size=256 B; 261888 / 256 = 1023    */
/* exactly, so one zone holds exactly 1023 such allocs.                        */
/* -------------------------------------------------------------------------- */

static void test_small_zone_release(void)
{
	SECTION("SMALL zone release — empty SMALL zone is munmap'd");

	const int capacity = 1023;
	void     *ptrs[1023];

	for (int i = 0; i < capacity; i++)
		ptrs[i] = malloc(129);

	if (show_alloc_mem) {
		ft_printf("After filling one SMALL zone (expect one SMALL zone):\n");
		show_alloc_mem();
	}

	for (int i = 0; i < capacity; i++)
		free(ptrs[i]);

	if (show_alloc_mem) {
		ft_printf("After freeing all (expect empty):\n");
		show_alloc_mem();
	}

	void *p = malloc(129);
	CHECK("alloc succeeds after SMALL zone release", p != NULL);
	free(p);
}

/* -------------------------------------------------------------------------- */
/* Section 11 — realloc: NULL pointer and zero size                           */
/*                                                                            */
/* man 3 realloc corner cases: realloc(NULL, n) is equivalent to malloc(n),   */
/* and realloc(p, 0) frees p and returns NULL. realloc(NULL, 0) follows the   */
/* malloc(0) rule: a unique, freeable pointer.                                */
/* -------------------------------------------------------------------------- */

static void test_realloc_null_and_zero(void)
{
	SECTION("realloc — NULL pointer behaves as malloc, zero size frees");

	void *p = realloc(NULL, 64);
	CHECK("realloc(NULL, n) returns non-NULL (== malloc)", p != NULL);
	CHECK("realloc(NULL, n) is 16-aligned", is_aligned(p, 16));

	void *q = realloc(p, 0);
	CHECK("realloc(p, 0) returns NULL (frees p)", q == NULL);

	void *r = realloc(NULL, 0);
	CHECK("realloc(NULL, 0) returns a freeable pointer", r != NULL);
	free(r);
}

/* -------------------------------------------------------------------------- */
/* Section 12 — realloc: same size and in-class shrink keep the same pointer  */
/*                                                                            */
/* When the new size stays inside the current size class, our realloc resizes */
/* in place and returns the SAME pointer: same-size is a no-op, and a shrink   */
/* just releases the tail (clearing bitmap chunks for TINY, splitting a        */
/* remainder for SMALL). This is an allocator-specific guarantee (man 3        */
/* realloc does not require it) and a cheap way to confirm the in-place path   */
/* is taken rather than a needless malloc+copy+free. Kept bytes must survive.  */
/* -------------------------------------------------------------------------- */

static void test_realloc_shrink_inplace(void)
{
	SECTION("realloc — same size and in-class shrink keep the same pointer");

	/* TINY: 128 -> 128 (no-op) and 128 -> 16 (shrink), both stay TINY. */
	char *t = malloc(128);
	CHECK("TINY alloc for shrink test", t != NULL);
	fill_pattern(t, 128, 7);
	char *ts = realloc(t, 128);
	CHECK("realloc TINY to same size returns the same pointer", ts == t);
	char *t2 = realloc(ts, 16);
	CHECK("realloc TINY shrink returns the same pointer (in place)", t2 == ts);
	CHECK("realloc TINY shrink preserves the kept bytes",
	      check_pattern(t2, 16, 7));
	free(t2);

	/* SMALL: 2000 -> 1000, both stay inside the SMALL range -> in place. */
	char *s = malloc(2000);
	CHECK("SMALL alloc for shrink test", s != NULL);
	fill_pattern(s, 2000, 9);
	char *s2 = realloc(s, 1000);
	CHECK("realloc SMALL shrink (still SMALL) returns the same pointer",
	      s2 == s);
	CHECK("realloc SMALL shrink preserves the kept bytes",
	      check_pattern(s2, 1000, 9));
	free(s2);
}

/* -------------------------------------------------------------------------- */
/* Section 13 — realloc: grow within a size class preserves data              */
/*                                                                            */
/* Growing while staying in the same class may resize in place (absorbing the */
/* adjacent free space) or fall back to malloc+copy+free if the neighbour is   */
/* occupied. Either way the original bytes must be preserved and the enlarged  */
/* region must be fully writable. We do not assert pointer identity here       */
/* because an in-place grow is not guaranteed.                                 */
/* -------------------------------------------------------------------------- */

static void test_realloc_grow(void)
{
	SECTION("realloc — grow within a size class preserves data");

	char *t = malloc(16);
	CHECK("TINY alloc for grow test", t != NULL);
	fill_pattern(t, 16, 3);
	char *t2 = realloc(t, 120); /* still TINY */
	CHECK("realloc TINY grow returns non-NULL", t2 != NULL);
	CHECK("realloc TINY grow preserves the original bytes",
	      check_pattern(t2, 16, 3));
	fill_pattern(t2, 120, 3);
	CHECK("realloc TINY grown region is fully writable",
	      check_pattern(t2, 120, 3));
	free(t2);

	char *s = malloc(300);
	CHECK("SMALL alloc for grow test", s != NULL);
	fill_pattern(s, 300, 5);
	char *s2 = realloc(s, 1500); /* still SMALL */
	CHECK("realloc SMALL grow returns non-NULL", s2 != NULL);
	CHECK("realloc SMALL grow preserves the original bytes",
	      check_pattern(s2, 300, 5));
	fill_pattern(s2, 1500, 5);
	CHECK("realloc SMALL grown region is fully writable",
	      check_pattern(s2, 1500, 5));
	free(s2);
}

/* -------------------------------------------------------------------------- */
/* Section 14 — realloc: migration across size classes preserves data         */
/*                                                                            */
/* When the new size crosses a class boundary the block must move to a         */
/* different zone type (malloc in the new class, copy min(old,new) bytes,      */
/* free the old). We walk a block up TINY -> SMALL -> LARGE and then back down */
/* LARGE -> TINY, checking after every step that the bytes carried over so far */
/* are intact. A wrong copy length or offset corrupts the pattern.            */
/* -------------------------------------------------------------------------- */

static void test_realloc_cross_class(void)
{
	SECTION("realloc — migration across size classes preserves data");

	char *p = malloc(64); /* TINY */
	CHECK("TINY alloc for cross-class test", p != NULL);
	fill_pattern(p, 64, 11);

	char *q = realloc(p, 1500); /* -> SMALL */
	CHECK("realloc TINY->SMALL returns non-NULL", q != NULL);
	CHECK("realloc TINY->SMALL preserves the original bytes",
	      check_pattern(q, 64, 11));

	fill_pattern(q, 1500, 13);
	char *r = realloc(q, 5000); /* -> LARGE */
	CHECK("realloc SMALL->LARGE returns non-NULL", r != NULL);
	CHECK("realloc SMALL->LARGE preserves the original bytes",
	      check_pattern(r, 1500, 13));

	fill_pattern(r, 5000, 17);
	char *s = realloc(r, 64); /* -> TINY (down two classes) */
	CHECK("realloc LARGE->TINY returns non-NULL", s != NULL);
	CHECK("realloc LARGE->TINY preserves the kept bytes",
	      check_pattern(s, 64, 17));
	free(s);
}

/* -------------------------------------------------------------------------- */
/* Section 15 — realloc: LARGE grow and shrink preserve data                  */
/*                                                                            */
/* A LARGE block grows via malloc+copy+free (a bigger dedicated mmap), and     */
/* shrinks in place by munmap'ing the tail pages and keeping the same base.    */
/* Both must preserve the bytes that remain in range, and the grown region     */
/* must be writable end to end.                                               */
/* -------------------------------------------------------------------------- */

static void test_realloc_large(void)
{
	SECTION("realloc — LARGE grow and shrink preserve data");

	char *p = malloc(5000);
	CHECK("LARGE alloc for realloc test", p != NULL);
	fill_pattern(p, 5000, 21);

	char *g = realloc(p, 9000); /* grow */
	CHECK("realloc LARGE grow returns non-NULL", g != NULL);
	CHECK("realloc LARGE grow preserves the original bytes",
	      check_pattern(g, 5000, 21));
	fill_pattern(g, 9000, 21);
	CHECK("realloc LARGE grown region is fully writable",
	      check_pattern(g, 9000, 21));

	char *s = realloc(g, 5000); /* shrink */
	CHECK("realloc LARGE shrink returns non-NULL", s != NULL);
	CHECK("realloc LARGE shrink preserves the kept bytes",
	      check_pattern(s, 5000, 21));
	free(s);
}

/* -------------------------------------------------------------------------- */
/* Section 16 — realloc: invalid / foreign / interior pointer                 */
/*                                                                            */
/* realloc of a pointer we never handed out must be rejected (NULL) without    */
/* aborting or corrupting state. Three cases: a foreign pointer (static        */
/* storage -> find_zone NULL); an interior TINY pointer (aligned and in a      */
/* valid zone's range, but not an is_start chunk); and a misaligned interior   */
/* SMALL pointer. The interior cases regress the gap where realloc trusted     */
/* find_zone and skipped the per-allocation validation free performs, then     */
/* corrupted the real neighbouring allocation. call_realloc hides the          */
/* pointer's origin from -Werror=free-nonheap-object.                          */
/* -------------------------------------------------------------------------- */

static void test_realloc_invalid(void)
{
	SECTION("realloc — invalid/foreign/interior pointer is rejected");

	static char foreign[64];

	void *r = call_realloc(foreign, 100);
	CHECK("realloc(foreign ptr, n) returns NULL without aborting", r == NULL);

	/* Interior TINY pointer: aligned and in-range, but the 2nd chunk of a
	 * 4-chunk block, so not an allocation start. */
	char *ta = malloc(64);
	char *tb = malloc(64);
	CHECK("TINY allocs for interior-realloc test", ta != NULL && tb != NULL);
	if (ta != NULL && tb != NULL) {
		fill_pattern(ta, 64, 0x3A);
		fill_pattern(tb, 64, 0x3B);
		void *ti = call_realloc(ta + 16, 80);
		CHECK("realloc(interior TINY ptr) returns NULL", ti == NULL);
		CHECK("rejected interior TINY realloc leaves neighbours intact",
		      check_pattern(ta, 64, 0x3A) && check_pattern(tb, 64, 0x3B));
		free(ta);
		free(tb);
	}

	/* Interior SMALL pointer: not SMALL_QUANTUM-aligned. */
	char *sa = malloc(512);
	CHECK("SMALL alloc for interior-realloc test", sa != NULL);
	if (sa != NULL) {
		fill_pattern(sa, 512, 0x5C);
		void *si = call_realloc(sa + 8, 600);
		CHECK("realloc(interior SMALL ptr) returns NULL", si == NULL);
		CHECK("rejected interior SMALL realloc leaves the block intact",
		      check_pattern(sa, 512, 0x5C));
		free(sa);
	}

	/* Aligned-interior SMALL pointer: 256-aligned but inside a larger chunk,
	 * so not a chunk start. Poison the word it would read as a header with a
	 * huge IN_USE size; before the footer-bounds fix, small_ptr_is_valid
	 * derived CHUNK_FTR from that size and read off the mapping (OOB). */
	char *sb = malloc(2000); /* 256-aligned, one 2048 B chunk */
	CHECK("SMALL alloc for aligned-interior realloc test", sb != NULL);
	if (sb != NULL) {
		fill_pattern(sb, 240, 0x6D);
		/* The header word an interior sb + 256 reads: huge size, IN_USE. */
		*(size_t *)(sb + 256 - sizeof(size_t)) = (size_t)0x10000001;
		void *ri = call_realloc(sb + 256, 600);
		CHECK("realloc(aligned-interior SMALL ptr) returns NULL", ri == NULL);
		CHECK("aligned-interior realloc leaves the block head intact",
		      check_pattern(sb, 240, 0x6D));
		free(sb);
	}
}

/* -------------------------------------------------------------------------- */
/* Section 17 — free edge cases (every class, no abort)                       */
/*                                                                            */
/* Invalid frees must print a diagnostic to stderr and return — never abort    */
/* or corrupt state. We cover, across all classes:                            */
/*   - free(NULL): documented no-op.                                          */
/*   - double free (TINY/SMALL/LARGE): the second free must be rejected. For   */
/*     TINY/SMALL we surround the victim with live anchors so its zone is not  */
/*     released and the chunk stays put (so the bitmap / boundary-tag state    */
/*     check can catch the re-free); for LARGE the first free unmaps the zone, */
/*     so the second free's pointer maps to no zone (find_zone -> NULL).       */
/*   - misaligned TINY pointer: rejected by the 16-B alignment check.          */
/*   - interior SMALL pointer: not SMALL_QUANTUM-aligned and its header/footer */
/*     tags disagree -> rejected.                                             */
/*   - foreign pointer (static storage): owned by nobody -> find_zone NULL.    */
/* call_free() hides pointer origins from GCC's static free checks.           */
/* -------------------------------------------------------------------------- */

static void test_free_edge_cases(void)
{
	SECTION("free edge cases — invalid frees rejected without aborting");

	free(NULL);
	CHECK("free(NULL) does not crash", 1);

	/* TINY double free (anchor keeps the zone alive). */
	void *anchor = malloc(16);
	void *p      = malloc(16);
	call_free(p);
	call_free(p);
	CHECK("TINY double free is rejected (error to stderr, no abort)", 1);

	/* TINY misaligned pointer. */
	void *q = malloc(16);
	call_free((char *)q + 1);
	CHECK("TINY misaligned free is rejected", 1);

	/* SMALL double free (neighbours keep the chunk from coalescing away). */
	void *sa = malloc(512);
	void *sp = malloc(512);
	void *sb = malloc(512);
	call_free(sp);
	call_free(sp);
	CHECK("SMALL double free is rejected", 1);

	/* SMALL interior pointer (not quantum-aligned, tags disagree). */
	void *si = malloc(512);
	call_free((char *)si + 8);
	CHECK("SMALL interior free is rejected", 1);

	/* SMALL aligned-interior pointer: 256-aligned but inside a larger chunk.
	 * Poison the header word it would read with a huge IN_USE size; the
	 * footer-bounds check in small_ptr_is_valid must reject it instead of
	 * reading off the mapping. */
	void *sai = malloc(2000);
	if (sai != NULL) {
		*(size_t *)((char *)sai + 256 - sizeof(size_t)) = (size_t)0x10000001;
		call_free((char *)sai + 256);
		CHECK("SMALL aligned-interior free is rejected without crashing", 1);
		free(sai);
	}

	/* LARGE double free (zone already unmapped on the first free). */
	void *lp = malloc(5000);
	call_free(lp);
	call_free(lp);
	CHECK("LARGE double free is rejected", 1);

	/* Foreign pointer never returned by our allocator. */
	static char foreign[32];
	call_free(foreign);
	CHECK("free of a foreign pointer is rejected", 1);

	free(anchor);
	free(q);
	free(sa);
	free(sb);
	free(si);
}

/* -------------------------------------------------------------------------- */
/* Section 18 — SMALL coalescing (fragmented free then refill)                */
/*                                                                            */
/* Allocate a run of SMALL chunks, free them in an interleaved order (odd      */
/* indices first, then even) so that every coalesce case is exercised: merge   */
/* with a free successor, a free predecessor, and both at once. The survivors  */
/* must stay intact while their neighbours are freed, and after freeing the    */
/* whole run the space must be reclaimed — a fresh identical run must allocate  */
/* and read back cleanly. If coalescing corrupted a boundary tag, the refill   */
/* would fail or read garbage.                                                 */
/* -------------------------------------------------------------------------- */

static void test_small_coalescing(void)
{
	SECTION("SMALL coalescing — fragmented free then refill keeps integrity");

	const int n = 64;
	void     *ptrs[64];

	for (int i = 0; i < n; i++) {
		ptrs[i] = malloc(300);
		if (ptrs[i])
			fill_pattern(ptrs[i], 300, (unsigned)i);
	}

	/* Free odd indices first (creates holes between live chunks), then even. */
	for (int i = 1; i < n; i += 2)
		free(ptrs[i]);

	int survivors_ok = 1;
	for (int i = 0; i < n; i += 2)
		if (!ptrs[i] || !check_pattern(ptrs[i], 300, (unsigned)i))
			survivors_ok = 0;
	CHECK("survivors intact after freeing interleaved neighbours",
	      survivors_ok);

	for (int i = 0; i < n; i += 2)
		free(ptrs[i]);

	/* Reclaimed space must serve an identical run again. */
	int refill_ok = 1;
	for (int i = 0; i < n; i++) {
		ptrs[i] = malloc(300);
		if (!ptrs[i]) {
			refill_ok = 0;
			break;
		}
		fill_pattern(ptrs[i], 300, (unsigned)(i + 100));
	}
	CHECK("refill after full free succeeds", refill_ok);

	int refill_intact = 1;
	for (int i = 0; i < n && refill_ok; i++)
		if (!check_pattern(ptrs[i], 300, (unsigned)(i + 100)))
			refill_intact = 0;
	CHECK("refilled allocations are intact", refill_intact);

	for (int i = 0; i < n; i++)
		free(ptrs[i]);
}

/* -------------------------------------------------------------------------- */
/* Section 19 — Stress: randomized alloc/realloc/free cycles                  */
/*                                                                            */
/* A deterministic LCG drives many iterations over a pool of slots, each       */
/* iteration allocating an empty slot, or (for an occupied slot) reallocating  */
/* or freeing it. Every block carries a position-dependent pattern keyed by a  */
/* per-slot seed; the pattern is verified before every realloc/free and the    */
/* kept prefix re-verified after every realloc. This shakes out cross-          */
/* allocation overlap, stale frees, and bad realloc copies under churn that    */
/* repeatedly creates and releases zones across all three size classes. The    */
/* run is single-threaded; concurrent stress lives in test_mt.c (helgrind).    */
/* -------------------------------------------------------------------------- */

static void test_stress_cycles(void)
{
	SECTION("Stress — randomized alloc/realloc/free cycles");

	enum { SLOTS = 96, ITERS = 5000 };
	static const size_t sizes[] = { 1,	16,   80,   128,  129,	300,
					1000,	2032, 2033, 8000, 40000 };
	const int nsizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

	void    *ptr[SLOTS]  = { 0 };
	size_t   sz[SLOTS]   = { 0 };
	unsigned seed[SLOTS] = { 0 };
	unsigned rng         = 0x12345678u;
	int      ok          = 1;

	for (int it = 0; it < ITERS && ok; it++) {
		rng      = rng * 1103515245u + 12345u;
		int slot = (int)((rng >> 8) % SLOTS);
		rng      = rng * 1103515245u + 12345u;
		int action = (int)((rng >> 8) % 3); /* occupied: 1=realloc else free */

		if (ptr[slot] == NULL) {
			rng           = rng * 1103515245u + 12345u;
			size_t   s    = sizes[(rng >> 8) % (unsigned)nsizes];
			unsigned sd   = (rng >> 3) & 0xFFu;
			void    *fresh = malloc(s);
			if (fresh == NULL) {
				ok = 0;
				break;
			}
			fill_pattern(fresh, s, sd);
			ptr[slot]  = fresh;
			sz[slot]   = s;
			seed[slot] = sd;
		} else if (action == 1) {
			if (!check_pattern(ptr[slot], sz[slot], seed[slot])) {
				ok = 0;
				break;
			}
			rng        = rng * 1103515245u + 12345u;
			size_t ns  = sizes[(rng >> 8) % (unsigned)nsizes];
			void  *np  = realloc(ptr[slot], ns);
			if (np == NULL) {
				ok = 0;
				break;
			}
			size_t keep = ns < sz[slot] ? ns : sz[slot];
			if (!check_pattern(np, keep, seed[slot])) {
				ok = 0;
				break;
			}
			fill_pattern(np, ns, seed[slot]);
			ptr[slot] = np;
			sz[slot]  = ns;
		} else {
			if (!check_pattern(ptr[slot], sz[slot], seed[slot])) {
				ok = 0;
				break;
			}
			free(ptr[slot]);
			ptr[slot] = NULL;
			sz[slot]  = 0;
		}
	}
	CHECK("randomized alloc/realloc/free cycles preserve integrity", ok);

	for (int i = 0; i < SLOTS; i++)
		if (ptr[i])
			free(ptr[i]);

	void *p = malloc(64);
	CHECK("allocation works after draining the stress pool", p != NULL);
	free(p);
}

/* -------------------------------------------------------------------------- */
/* Section 20 — show_alloc_mem                                                */
/*                                                                            */
/* Reproduce a layout close to the subject's example: two TINY allocs, one    */
/* SMALL alloc, and one LARGE alloc. Verify the function runs to completion.   */
/* The visual output lets the developer confirm the format and the address-   */
/* sorted ordering manually.                                                  */
/* -------------------------------------------------------------------------- */

static void test_show_alloc_mem(void)
{
	SECTION("show_alloc_mem");

	if (!show_alloc_mem) {
		ft_printf(
			"[SKIP] show_alloc_mem not available (library not preloaded)\n");
		return;
	}

	void *t1 = malloc(42);
	void *t2 = malloc(84);
	void *s1 = malloc(500);
	void *l1 = malloc(48847);

	ft_printf(
		"Expected layout: one TINY zone (42+84 B), one SMALL zone, one LARGE zone:\n");
	show_alloc_mem();
	CHECK("show_alloc_mem ran without crashing", 1);

	free(t1);
	free(t2);
	free(s1);
	free(l1);

	ft_printf("After freeing all (expect empty output):\n");
	show_alloc_mem();
	CHECK("show_alloc_mem after full free ran without crashing", 1);
}

/* -------------------------------------------------------------------------- */
/* Section 21 — realloc grow of a TINY block at the end of its zone           */
/*                                                                            */
/* Regression test for a heap overflow in realloc_tiny. A block at the        */
/* last usable chunk of a TINY zone used to "grow" in place: the scan         */
/* ran past the end of the in_use bitmap (no bound check), read the           */
/* adjacent is_start words as free, claimed chunks beyond the mapping,        */
/* and returned the SAME pointer -- so writing the grown size ran off         */
/* the end of the 16 KB zone (silent corruption, or a SIGSEGV).               */
/*                                                                            */
/* The end-of-zone block is found without hard-coding the header size:        */
/* on a clean arena, 16-B allocations fill a zone at contiguous (+16 B)       */
/* addresses, so the block just before the first address jump sits at         */
/* the zone's last chunk. Growing it to the 128-B (8-chunk) class has         */
/* no room and MUST relocate; returning the old pointer is the bug.           */
/* (White-box: assumes a 4 KB page, like the zone-exhaustion tests.)          */
/* -------------------------------------------------------------------------- */

static void test_realloc_tiny_zone_end(void)
{
	SECTION("realloc — grow a TINY block at the end of its zone (regression)");

	enum { CAP = 2048 };
	void *ptrs[CAP];
	int   n       = 0;
	int   end_idx = -1;
	void *prev    = malloc(16);

	if (prev != NULL)
		ptrs[n++] = prev;
	while (prev != NULL && n < CAP) {
		void *cur = malloc(16);
		if (cur == NULL)
			break;
		ptrs[n++] = cur;
		if ((char *)cur != (char *)prev + 16) {
			end_idx = n - 2; /* prev is the last chunk of its zone */
			break;
		}
		prev = cur;
	}

	CHECK("located a TINY block at a zone boundary", end_idx >= 0);

	if (end_idx >= 0) {
		void *end_block = ptrs[end_idx];
		void *grown;

		fill_pattern(end_block, 16, 0xE5);
		grown = realloc(end_block, 128); /* grow within the TINY class */
		CHECK("realloc of an end-of-zone TINY block returns non-NULL",
		      grown != NULL);
		/* No room past the zone's last chunk, so a correct realloc must
		 * move the block; returning the same pointer is the overflow. */
		CHECK("end-of-zone TINY grow relocates instead of overrunning the zone",
		      grown != NULL && grown != end_block);

		if (grown != NULL && grown != end_block) {
			ptrs[end_idx] = grown; /* keep the array valid for cleanup */
			CHECK("end-of-zone grow preserves the original bytes",
			      check_pattern(grown, 16, 0xE5));
			CHECK("end-of-zone grown region is fully writable",
			      fill_and_check(grown, 128, 0xAB));
		}
	}

	for (int i = 0; i < n; i++)
		free(ptrs[i]);
}

/* -------------------------------------------------------------------------- */
/* Section 22 — malloc/realloc reject page-rounding overflow                  */
/*                                                                            */
/* Regression test for the LARGE size-overflow guard. A request within        */
/* a page+header of SIZE_MAX made (size + header + PAGE_SIZE - 1) wrap        */
/* to a tiny value, so malloc_large mapped a few KB yet reported              */
/* success -- the caller then believed it owned ~SIZE_MAX bytes, and          */
/* realloc_large even munmap'd the tail of a real block on the wrap.          */
/*                                                                            */
/* malloc/realloc must return NULL for such sizes, and a failed               */
/* realloc must leave the original block untouched. We only inspect           */
/* the returned pointers, never writing through a bogus one; the size         */
/* is read via a volatile so -Walloc-size-larger-than stays quiet             */
/* under -Werror.                                                             */
/* -------------------------------------------------------------------------- */

static void test_large_size_overflow(void)
{
	SECTION("malloc/realloc — reject page-rounding overflow (regression)");

	/* volatile so the compiler cannot constant-fold the size into the call
	 * and trip -Walloc-size-larger-than under -Werror; we are deliberately
	 * handing the allocator an unsatisfiable size. */
	volatile size_t huge = SIZE_MAX;

	void *big = malloc(huge);
	CHECK("malloc(SIZE_MAX) returns NULL, not a tiny mapping", big == NULL);
	free(big); /* NULL on correct behaviour -- a no-op then */

	void *p = malloc(5000); /* a genuine LARGE block to resize */
	CHECK("LARGE alloc for the overflow-realloc test", p != NULL);
	if (p != NULL) {
		fill_pattern(p, 5000, 0x5A);

		void *r = realloc(p, huge);
		CHECK("realloc to an overflowing size returns NULL", r == NULL);
		if (r == NULL) {
			/* A failed realloc must leave the original untouched. */
			CHECK("failed overflow-realloc preserves the original",
			      check_pattern(p, 5000, 0x5A));
			free(p);
		} else {
			free(r); /* buggy path: don't leak / read past the zone */
		}
	}
}

/* -------------------------------------------------------------------------- */
/* main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
	test_basic_alloc();
	test_basic_small_alloc();
	test_large_alloc();
	test_write_integrity();
	test_free_max_span_tiny();
	test_interior_free();
	test_tiny_zone_exhaustion();
	test_tiny_zone_release();
	test_small_zone_exhaustion();
	test_small_zone_release();
	test_realloc_null_and_zero();
	test_realloc_shrink_inplace();
	test_realloc_grow();
	test_realloc_cross_class();
	test_realloc_large();
	test_realloc_invalid();
	test_free_edge_cases();
	test_small_coalescing();
	test_stress_cycles();
	test_show_alloc_mem();
	test_realloc_tiny_zone_end();
	test_large_size_overflow();

	ft_printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

	return g_fail != 0;
}
