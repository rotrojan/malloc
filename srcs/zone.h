#ifndef ZONE_H
#define ZONE_H

#include <stddef.h> /* for size_t */
#include <stdint.h> /* for uint64_t */

#define MAGIC 0xdeadbeeff00dbabe

typedef enum zone_type {
	TINY_ZONE,
	SMALL_ZONE,
	LARGE_ZONE,
	ZONE_TYPE_MAX
} e_zone_type;

typedef struct zone_hdr {
	uint64_t    magic;
	e_zone_type type;
	size_t      size;
	uintptr_t   self;
	uint64_t    checksum;
} s_zone_hdr;

void *new_zone(e_zone_type zone_type, size_t size);

#endif /* ZONE_H */
