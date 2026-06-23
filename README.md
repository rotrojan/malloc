# ft_malloc — a `malloc`/`free`/`realloc` replacement

A drop-in dynamic memory allocator, built as a shared library and injected into
any program with `LD_PRELOAD`. It implements `malloc(3)`, `free(3)` and
`realloc(3)` (plus the subject's `show_alloc_mem()`), backs everything with
`mmap(2)`/`munmap(2)` only, and is **thread-safe**.

This is the 42 *malloc* project.

---

## Table of contents

- [Quick start](#quick-start)
- [The big picture](#the-big-picture)
- [Size classes](#size-classes)
- [Zones: the unit of memory](#zones-the-unit-of-memory)
- [The three allocators](#the-three-allocators)
  - [TINY — bitmap allocator](#tiny--bitmap-allocator)
  - [SMALL — boundary tags + segregated bins](#small--boundary-tags--segregated-bins)
  - [LARGE — one mapping per allocation](#large--one-mapping-per-allocation)
- [Finding a pointer's zone](#finding-a-pointers-zone)
- [Thread safety](#thread-safety)
- [Out-of-memory (OOM) accounting via `--wrap`](#out-of-memory-oom-accounting-via---wrap)
- [Symbol visibility](#symbol-visibility)
- [Design choices & trade-offs](#design-choices--trade-offs)
- [Testing](#testing)
- [Source layout](#source-layout)
- [Subject compliance checklist](#subject-compliance-checklist)

---

## Quick start

```sh
make                       # builds libft_malloc_$(uname -m)_$(uname -s).so + the libft_malloc.so symlink
make DEBUG=1               # same, with -g3 debug symbols
make run_test              # build + run the single-threaded test suite (123 checks)
make run_test_mt           # build + run the multithreaded stress test under helgrind
make run_test_oom          # build + run the out-of-memory / allocation-failure test
```

Use it against any program:

```sh
LD_PRELOAD=./libft_malloc.so ls -l
```

Larger programs work too — but only with an `EXTRA` build, which also provides
`calloc`/`reallocarray` (see [Design choices](#design-choices--trade-offs)):

```sh
make EXTRA=1
LD_PRELOAD=./libft_malloc.so vim
```

A default build exports exactly four symbols: `malloc`, `free`, `realloc`,
`show_alloc_mem` (an `EXTRA` build adds `calloc` and `reallocarray`).
`show_alloc_mem()` prints every live allocation, grouped by zone and sorted by
increasing address:

```
TINY : 0xA0000
0xA0020 - 0xA004A : 42 bytes
SMALL : 0xAD000
0xAD020 - 0xADEAD : 3725 bytes
LARGE : 0xB0000
0xB0020 - 0xBBEEF : 48847 bytes
Total : 52698 bytes
```

---

## The big picture

Every byte the allocator hands out lives inside a **zone** — a page-aligned
region obtained from `mmap`. A request is routed to one of three allocators by
size, each tuned to a different regime:

```
malloc(size)
   │
   ├─ size ≤ 128 B ........ TINY   → fixed 16 B chunks, tracked by a bitmap
   ├─ size ≤ 2032 B ....... SMALL  → variable chunks, boundary tags + free bins
   └─ size  > 2032 B ...... LARGE  → its own dedicated mmap, one per allocation
```

Two layers of bookkeeping sit on top:

- a **global registry** (`g_malloc_state.zone_list`): an address-ordered,
  doubly-linked list of *every* zone, used to map an arbitrary pointer back to
  the zone that owns it (`free`/`realloc`) and to walk everything for
  `show_alloc_mem`;
- a fixed pool of **arenas** (`g_arena[8]`): independent shards, each with its
  own TINY/SMALL zones and its own lock, so threads rarely contend.

There are exactly **two global variables**, as the subject requires: one for
allocation state (`g_malloc_state`) and one for thread-safety (`g_arena`).

---

## Size classes

The subject leaves the four size parameters to the student: `n` (TINY ceiling),
`m` (SMALL ceiling), `N` (TINY zone size), `M` (SMALL zone size). The choices
here, and the constraint that drives them — *each TINY and SMALL zone must hold
at least 100 allocations of its maximum size*:

| Class | Request size  | Chunk model            | Zone size        | Max-size chunks/zone | ≥ 100? |
|-------|---------------|------------------------|------------------|----------------------|--------|
| TINY  | `0 … 128 B`   | 16 B granularity       | 4 pages (16 KB)  | 128 → ~1004 usable   | ✅     |
| SMALL | `129 … 2032 B`| 256 B quantum          | 64 pages (256 KB)| 127                  | ✅     |
| LARGE | `> 2032 B`    | exact, page-rounded    | one mmap each    | n/a                  | n/a    |

Where the numbers come from (all assuming a 4 KB page):

- **TINY ceiling = 128 B, granularity = 16 B.** A 128 B allocation spans
  `128 / 16 = 8` chunks — the maximum a single TINY allocation can occupy. A
  4-page zone holds `16384 / 16 = 1024` chunks; the header eats the first 20, so
  ~1004 are usable — comfortably ≥ 100 even at the 128 B maximum.
- **SMALL ceiling = 2032 B.** That's `8 × 256 − 2 × TAG_SIZE`: eight 256 B
  quanta minus the per-chunk boundary tags (8 B each). A 64-page zone has a
  261 888 B usable span; at the 2048 B maximum real chunk size that's 127
  chunks — ≥ 100.
- **LARGE** is everything bigger, where per-allocation `mmap` is the right tool.

The boundary between TINY and SMALL (128 vs 129) and SMALL and LARGE (2032 vs
2033) are exact and tested.

---

## Zones: the unit of memory

Every zone begins with an `s_zone_hdr` (`srcs/zone.h`):

```c
typedef struct zone_hdr {
    uint64_t         magic;     // 0xdeadbeeff00dbabe — sanity / corruption guard
    e_zone_type      type;      // TINY_ZONE | SMALL_ZONE | LARGE_ZONE
    size_t           size;      // total mapped bytes
    uintptr_t        self;      // == (uintptr_t)this header (detects relocation)
    uint64_t         checksum;  // magic ^ type ^ self ^ size
    struct arena    *arena;     // owning arena (NULL for LARGE)
    struct zone_hdr *prev;      // global registry links (doubly-linked)
    struct zone_hdr *next;
} s_zone_hdr;
```

The header is validated on every `free`/`realloc` (`zone_is_valid`): the magic,
the self-pointer, the type, the size, an address-range bounds check, and the XOR
checksum must all agree. A foreign pointer (for example, a glibc allocation made
before our hook was installed) fails these checks and is politely rejected to
`stderr` instead of corrupting state.

> **Two link memberships, two pointer pairs.** A TINY/SMALL zone lives in *two*
> lists at once: the global registry (via `s_zone_hdr.{prev,next}`) and its
> arena's own list (via `s_tiny_zone`/`s_small_zone`'s *own* `prev`/`next`). A
> single link pair can only thread a node through one list, so each zone carries
> two. Both lists are doubly-linked, so a zone located by `find_zone` is
> unlinked from both in **O(1)** on release. The XOR checksum deliberately
> covers only `magic ^ type ^ self ^ size`, *not* the link pointers, so relinking
> never needs a checksum recompute.

---

## The three allocators

### TINY — bitmap allocator

`srcs/tiny.c`, `srcs/tiny.h`. A TINY zone is 4 pages and carved into 16 B
chunks. Two `uint64_t[16]` bitmaps track state:

- `in_use[]` — 1 if a chunk is allocated;
- `is_start[]` — 1 on the *first* chunk of each allocation.

```
chunk:     0    1    2    3    4    5   ...
in_use:    1    1    1    0    1    1
is_start:  1    0    0    0    1    0
           └──────────┘         └────┘
           alloc #1 (3 chunks)  alloc #2 (2 chunks, …)
```

Allocation is a **first-fit** search for `ceil(size/16)` consecutive zero bits
(`bitmap_find_consecutive_zeros`), accelerated by two tricks: a per-zone
`index_next_free_chunk` lower-bound hint, and a skip-ahead that jumps over runs
of set bits a byte at a time. `is_start` lets `free` recover an allocation's
length (count consecutive `in_use && !is_start` chunks) and reject a pointer
that lands in the *middle* of an allocation. When a free empties the whole
`in_use` bitmap, the zone is `munmap`'d immediately.

### SMALL — boundary tags + segregated bins

`srcs/small.c`, `srcs/small.h`. A boundary-tag allocator with segregated free
lists — the classic design taught in *Computer Systems: A Programmer's
Perspective* (CS:APP) by Bryant & O'Hallaron, chapter 9 (dynamic storage
allocation). Every chunk carries an identical 8-byte tag at its **head** and
**foot** encoding `size | in_use` (size is 8-aligned, so the low bits are free
for flags):

```
            ┌── header tag ──┐                 ┌── footer tag ──┐
 … prev …   │  size | IN_USE │   payload …     │  size | IN_USE │   next …
            └────────────────┘                 └────────────────┘
   PREV_CHUNK ◄── reached via the previous chunk's footer
                                NEXT_CHUNK ──► reached via this chunk's header
```

The footer is what makes **O(1) coalescing** possible: from any chunk you can
read the *previous* chunk's footer to learn its size and state, and merge across
boundaries in all four adjacency cases (prev free, next free, both, neither).
This boundary-tag technique is the central idea borrowed from CS:APP.

Free chunks are held in **9 segregated bins** by size class (256 B … 2048 B,
plus one oversized bin). A fresh zone starts as a single "wild" chunk filling
the whole zone in the oversized bin; allocation splits it, `free` re-inserts and
coalesces. A zone is released when a coalesce reconstitutes the full wild chunk.

`free_small` validates aggressively before touching anything: the pointer must
be `SMALL_QUANTUM`-aligned, its header must read `IN_USE` (catches
double-free), and its header and footer tags must agree (the boundary-tag
analogue of TINY's `is_start` check — catches interior/foreign pointers).

### LARGE — one mapping per allocation

`srcs/large.c`. Each LARGE allocation is its own zone, sized up to the next page
boundary, never attached to an arena. The user pointer is
`zone + sizeof(s_zone_hdr)`. `free` sends it straight to `munmap`. `realloc`
grows by allocating a fresh mapping and copying, but **shrinks in place** by
`munmap`-ing the tail pages and keeping the same base address.

---

## Finding a pointer's zone

`free(ptr)` and `realloc(ptr, …)` must answer "which zone owns `ptr`?".
`find_zone` walks the address-ordered global registry, stopping early once a
zone's base exceeds `ptr`, and validates each candidate with `zone_is_valid`.

This walk is **O(Z)** in the number of zones and runs on every `free`/`realloc`.
That cost is **kept on purpose**: a radix/page-table index could make it O(1),
but it would need an `mmap`-backed node allocator and per-page indexing — too
much machinery for the gain on a student project. A linked list has no random
access, so the lookup can't be made sublinear without replacing the list, and
that trade was consciously declined. (The *release* path, by contrast, is O(1):
the doubly-linked lists let a found zone be unlinked without a second walk.)

---

## Thread safety

The allocator is sharded into a fixed pool of **8 arenas** (`g_arena`,
`srcs/arena.c`). There is **no thread-local storage (TLS)**: a thread derives a home
arena from a hash of its thread id (`get_home_arena_idx(pthread_self())` modulo
the 8 arenas) and reaches it through `get_arena()`, which returns an arena
**already locked**:

1. try the home arena's lock (`pthread_mutex_trylock`);
2. on contention, `trylock`-hop around the rest of the pool;
3. only if every arena is busy, block on the home arena.

There are three locks, and they form a strict order — a directed acyclic graph
(DAG) of lock dependencies — always taken arena → registry → stat, never in
reverse:

| Lock                          | Protects                                                              |
|-------------------------------|----------------------------------------------------------------------|
| `s_arena.mutex` (per arena)   | that arena's zone lists, those zones' internal state, *and lifetime*  |
| `g_malloc_state.list_mutex`   | the global registry (`zone_list`) and LARGE-zone header mutation      |
| `g_malloc_state.stat_mutex`   | the `current_memory` counter (a leaf, taken only in the mmap wrappers)|

The locking contract in one line: **alloc may hop, free/realloc must go home.**
`malloc_tiny`/`malloc_small` take *any* arena via `get_arena()`. `free`/`realloc`
lock the *owning* arena (`zone->arena`) — never `get_arena()` — because they
target one specific existing zone.

Because an arena's mutex lives in `g_arena` (which outlives every zone), it can
be held *across* a zone's `munmap`. That is what lets reclamation be eager and
need no tombstone flag: the **pin invariant** — a valid `free`/`realloc` holds a
live pointer, so no other thread can empty or unmap that zone between
`find_zone` returning and the owning arena being locked. One arena lock covers
both the allocation scan and reclamation, closing the race between allocating
into a zone and another thread reclaiming it.

This model is validated under `valgrind --tool=helgrind`: 16 threads (more than
the 8 arenas, to force contention and the trylock-hop path), a churn phase, then
a cross-thread free phase — **0 data races, 0 lock-order inversions**.

---

## Out-of-memory (OOM) accounting via `--wrap`

The library is linked with `-Wl,--wrap=mmap -Wl,--wrap=munmap`. Every internal
`mmap`/`munmap` is redirected to `__wrap_mmap`/`__wrap_munmap`
(`srcs/stat_mmap.c`), which keep a running `current_memory` total and enforce an
OOM ceiling read from `getrlimit(RLIMIT_AS)` at init. This keeps the
accounting correct without sprinkling counter updates across every call site,
and `__real_mmap`/`__real_munmap` reach the genuine syscalls.

---

## Symbol visibility

A naïve shared library would export *everything* with external linkage — the
internal workers, the bitmap helpers, all of libft, even the globals. A linker
**version script** (`libft_malloc.map`, wired with `-Wl,--version-script`) marks
the public symbols `global` (`malloc`, `free`, `realloc`, `show_alloc_mem`, plus
`calloc`/`reallocarray` in an `EXTRA` build) and everything else `local`. The
map lists `calloc`/`reallocarray` unconditionally; the linker simply ignores
them in a default build where they are not compiled, so only four symbols are
exported. Verify with:

```sh
nm -D --defined-only libft_malloc.so   # → the four public symbols (six with EXTRA)
```

A version script (rather than `-fvisibility=hidden`) is used because it also
hides the symbols pulled in from the libft *static archive*, which the
compiler-level flag would not. Note that visibility only governs the *dynamic*
symbol table — intra-library calls between workers are unaffected — and the
`__wrap_*` shims going `local` is correct, since `--wrap` is resolved at
static-link time and never needed dynamic export.

---

## Design choices & trade-offs

- **Bitmap for TINY, boundary tags for SMALL.** TINY is dominated by many tiny,
  uniform objects, where a dense bitmap gives near-zero per-object metadata and
  cache-friendly scans. SMALL has larger, variable objects where boundary tags
  buy O(1) coalescing and in-place resize at the cost of 16 B/chunk overhead — a
  good trade only once chunks are large enough to amortize it.
- **A fixed pool of stateless arenas.** Sharding the allocator into 8 independent
  arenas, each with its own lock and zones, lets threads work in parallel with
  little contention. A thread picks an arena by hashing its id and may hop to a
  neighbour when its home is busy, so there is no per-thread state to allocate or
  tear down.
- **`find_zone` left O(Z).** See [above](#finding-a-pointers-zone) — a conscious
  trade, not an oversight. The redundant *second* walk on release was eliminated
  (doubly-linked lists, O(1) unlink); the single remaining lookup walk was kept.
  Two registry-free O(1) alternatives were considered and rejected. A *backward
  page-walk* (round `ptr` down to its page and step back until the zone magic
  appears) crashes on the foreign pointers glibc hands us before the
  `LD_PRELOAD` hook is live, and would need the forbidden `mincore(2)` to probe
  page residency safely. *Zone-size alignment* (align each zone to its own size
  so `ptr & ~(ZONE_SIZE - 1)` is the header) costs an over-map plus two trimming
  `munmap`s per zone, against the syscall budget. The address-ordered list keeps
  zone creation at a single `mmap` and pays only on lookup.
- **Eager `munmap` with no tombstone flag.** Justified by the pin invariant
  described under [Thread safety](#thread-safety); it keeps the resident set
  tight without a deferred-reclaim scheme.
- **`calloc`/`reallocarray` are opt-in (`make EXTRA=1`), off by default.** They
  are not part of the subject, and exporting `calloc` under `LD_PRELOAD` is
  risky: glibc internals (for example, name-service lookups during `ls -l`) hand
  our pointers back into `malloc_usable_size()`, which parses them as glibc
  chunks and crashes. So a default build omits them and lets the dynamic linker
  fall through to glibc. An `EXTRA` build (`-DEXTRA`) compiles and exports them,
  accepting that risk in exchange for running larger programs such as `vim` end
  to end on the allocator.

### Strengths

- Correct and complete `malloc`/`free`/`realloc` across all three size classes,
  proper alignment, and the required `show_alloc_mem`.
- Real thread safety (the bonus), proven race-free under helgrind.
- Defensive `free`/`realloc`: misaligned, interior, double-free and foreign
  pointers are all detected and reported, never aborted on.
- SMALL defragmentation via boundary-tag coalescing and in-place resize.
- Corruption detection on every operation (magic + self-pointer + checksum).
- A clean public application binary interface (ABI) — four exported symbols —
  and accurate memory accounting.

### Weaknesses & limitations

- **`free`/`realloc` are O(Z) and serialized on one lock.** Every `free`/`realloc`
  walks the global registry under a single `list_mutex`. So beyond the per-arena
  sharding of `malloc`, the deallocation path is both linear in the number of
  zones and a global serialization point — workloads that keep many zones live
  (especially many concurrent LARGE allocations) pay for both.
  *Fix:* replace the list with an address-keyed index (a page → zone radix tree,
  or a sorted array searched by binary search) for O(1)/O(log Z) lookup, and shard
  or make it lock-free for reads (per-arena sub-registries, or a seqlock so
  `find_zone` doesn't serialize on a global mutex).
- **Internal fragmentation from rounding.** TINY rounds up to 16 B, SMALL to the
  256 B quantum, LARGE to the page — so a 129 B request occupies a 256 B chunk
  and a 4097 B request occupies two pages. SMALL also spends 16 B of boundary
  tags per chunk.
  *Fix:* the 16 B TINY quantum is a floor, not a knob — returned pointers must be
  16-byte aligned (`_Alignof(max_align_t)`), so it cannot shrink without breaking
  the alignment guarantee. The real lever is SMALL: a smaller quantum / more size
  classes would tighten its up-to-255 B rounding, and dropping the footer on
  in-use chunks (storing the previous chunk's in-use bit in the next header — the
  footerless boundary-tag trick from CS:APP) would halve its per-chunk overhead.
- **No fast path / thread cache.** Even an uncontended `malloc` takes an arena
  `trylock`; there is no lock-free per-thread cache for the common case, so
  single-threaded throughput trails allocators that have one.
  *Fix:* add a small per-thread cache of recently-freed chunks (a `tcache`-style
  free list) served without any lock, falling back to the arenas on a miss —
  weighed against the subject's "one global for thread safety" rule, since a
  thread cache reintroduces per-thread state.
- **Fixed pool of 8 arenas.** The count is not tuned to the core count; past
  ~8 busy threads, contention and trylock-hopping rise.
  *Fix:* size the pool at init from `sysconf(_SC_NPROCESSORS_ONLN)` (clamped to a
  sane maximum) instead of a hard-coded 8.
- **LARGE does no caching.** Each LARGE `malloc`/`free` is a direct
  `mmap`/`munmap` syscall with no reuse, and any zone is only returned to the OS
  once fully empty (no partial decommit / `MADV_DONTNEED`).
  *Fix:* keep a small size-bucketed cache of recently-freed large mappings to
  reuse instead of unmapping, and/or `madvise(MADV_DONTNEED)` to release physical
  pages while retaining the virtual mapping — trading a little memory for far
  fewer syscalls.
- **Eager release churns syscalls at the empty/full boundary.** Emptied TINY/SMALL
  zones are unmapped immediately (the footprint-first trade under
  [Design choices](#design-choices--trade-offs)), so a workload that repeatedly
  drives a single zone across the empty/full line pays an `munmap` then an `mmap`
  every cycle. The pathological case is a single-chunk `malloc`/`free`
  ping-pong: each `free` empties the only zone and unmaps it, and the next
  `malloc` has to map a fresh one — turning what could be two pointer writes into
  two syscalls per iteration.
  *Fix:* give each arena a one-zone hysteresis — retain the first emptied zone as
  a spare and only `munmap` once a *second* zone goes idle — so the common
  boundary crossing reuses the spare without a syscall.
- **SMALL bins are singly-linked, and the oversized bin is first-fit.**
  Insertion and pop (`add_to_bin`/`pop_from_bin`) are O(1); only
  `remove_from_bin` — called from `coalesce`/`resize_chunk` on the free path —
  walks the bin to find a predecessor, so it is O(bin length). That only bites
  when a bin grows long under same-size fragmentation (many same-class holes in
  one zone that never coalesce); with adjacent frees merged, bins usually stay
  short. Note the eight *exact* classes are already best-fit by construction
  (you take the exact class, or the smallest larger one), so the only first-fit
  choice is in `BIN_OVERSIZED`, whose variable-size chunks are popped head-first
  regardless of how much they overshoot the request.
  *Fix:* doubly-link the free lists so `remove_from_bin` is O(1) too — nearly
  free, a 240 B-plus payload easily holds a second pointer; and for
  `BIN_OVERSIZED`, pick the smallest chunk that fits (best-fit) or keep it
  address-ordered, to shrink the split remainders it leaves behind.

---

## Testing

Three independent suites:

- **`test.c` (`make run_test`)** — 22 sections, 123 checks, single-threaded.
  Covers basic allocation and alignment per class; write-integrity across
  classes; zone exhaustion and release; the full `realloc` matrix (NULL/zero,
  in-place shrink/grow, cross-class migration, LARGE resize, invalid pointer);
  free edge cases across every class (double-free, misaligned, interior,
  foreign); SMALL coalescing; and a randomized alloc/realloc/free stress loop.
- **`test_mt.c` (`make run_test_mt`)** — 16-thread stress test run under
  `helgrind` with `--soname-synonyms=somalloc=NONE` (so *our* allocator runs
  rather than valgrind's). Churn phase + barrier + cross-thread frees.
- **`test_oom.c` (`make run_test_oom`)** — allocation-failure test: lowers
  `RLIMIT_AS`, then checks `malloc` returns `NULL` *gracefully* (no crash/abort)
  at the address-space ceiling and that freeing recovers. Exercises the
  failure-propagation path (`__wrap_mmap` → `new_zone` → `malloc` → `NULL`) the
  other suites never hit; it can't isolate the library's own counter from the
  kernel's `RLIMIT_AS` enforcement, and SKIPs if the environment leaves no
  headroom.

---

## Source layout

```
include/malloc.h          public prototypes (malloc/free/realloc)
libft_malloc.map          linker version script (the 4 exported symbols)
Makefile                  build, run_test, run_test_mt, run_test_oom
srcs/
  malloc.c                public malloc/free/realloc, init, dispatch
  inc/malloc_state.h      the g_malloc_state global + PAGE_SIZE
  inc/helpers.h           MIN/MAX/DIV_CEIL/ARRAY_SIZE macros
  zone.c / zone.h         zones, the global registry, find_zone/zone_is_valid
  arena.c / arena.h       the 8-arena pool and get_arena()
  tiny.c / tiny.h         TINY bitmap allocator
  small.c / small.h       SMALL boundary-tag allocator + bins
  large.c / large.h       LARGE per-mapping allocator
  bitmap.c / bitmap.h     uint64_t[] bitmap primitives
  stat_mmap.c             __wrap_mmap/__wrap_munmap (memory accounting)
  show_alloc_mem.c        the required live-allocation dump
libft/                    a vendored libc subset (ft_printf, ft_memcpy, …)
test.c / test_mt.c /      the three test suites
  test_oom.c
```

---

## Subject compliance checklist

- ✅ `malloc(3)`, `free(3)`, `realloc(3)` as an `LD_PRELOAD` shared library.
- ✅ Memory only from `mmap(2)`; released only with `munmap(2)`; no libc malloc.
- ✅ Zone sizes are multiples of `sysconf(_SC_PAGESIZE)`.
- ✅ TINY and SMALL zones each hold ≥ 100 max-size allocations.
- ✅ LARGE allocations get a dedicated mapping each.
- ✅ Returned memory is 16-byte aligned.
- ✅ Exactly one global for allocation state, one for thread safety.
- ✅ Only allowed functions used (`mmap`, `munmap`, `sysconf`, `getrlimit`,
  libft, libpthread).
- ✅ `show_alloc_mem()` implemented in the required format.
- ✅ **Bonus:** thread safety via libpthread (helgrind-clean).
