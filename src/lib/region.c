/*
 * This slightly-more-sophisticated-than-sillymem region-based allocator
 * is still pretty silly. The pools are linked lists of fairly-large
 * chunks, and allocations from a pool come from the those chunks; if
 * no space is available in a pool for an allocation, a new chunk is
 * added to the chain.
 *
 * Each pool also has a chain of jumbo-chunks, which are allocated only
 * for a single very-large allocation.
 */

#include "region.h"

#define CHUNK_SIZE	65536

typedef struct _RegionChunk
{
	struct _RegionChunk* next;
	size_t size;
	size_t used;
	size_t unused;
} RegionChunkHeader;

typedef union
{
	RegionChunkHeader chunk;
	uint8_t data[1];
} RegionChunk;

typedef struct
{
	RegionChunk* chunks;
	RegionChunk* jumbo;
} Pool;

static RegionChunk* newchunk(size_t size)
{
	RegionChunk* r = (RegionChunk*)malloc(size);
	if (!r)
	{
		return r;
	}

	r->chunk.next = NULL;
	r->chunk.size = size;

	// Align 8
	if (sizeof(RegionChunk) & 0x7)
	{
		r->chunk.used = (sizeof(RegionChunk) + 8) & ~0x7;
	}
	else
	{
		r->chunk.used = sizeof(RegionChunk);
	}

	r->chunk.unused = 0xdeadbeef;
	return r;
}

LillyPool leaf_newpool()
{
	Pool* pool = malloc(sizeof(Pool));
	if (!pool)
	{
		return NULL;
	}
	pool->jumbo = NULL;
	pool->chunks = newchunk(CHUNK_SIZE);

	return (LillyPool)pool;
}

static void freechunks(RegionChunk* r)
{
	while (r)
	{
		RegionChunk* next = (RegionChunk*)r->chunk.next;
		free(r);
		r = next;
	}
}

void leaf_endpool(LillyPool lpool)
{
	Pool* pool = (Pool*)lpool;

	if (!pool)
	{
		return;
	}
	freechunks(pool->chunks);
	freechunks(pool->jumbo);
	free(pool);
}

void* leaf_alloc(LillyPool lpool, size_t szbytes)
{
	Pool* pool = (Pool*)lpool;
	if (!pool)
	{
		return NULL;
	}
	RegionChunk* r = pool->chunks;

	while (r)
	{
		if (r->chunk.used + szbytes < r->chunk.size)
		{
			void* p = &(r->data[r->chunk.used]);
			r->chunk.used += szbytes;
			return p;
		}

		r = (RegionChunk*)r->chunk.next;
	}

	if (szbytes < CHUNK_SIZE - 2 * sizeof(RegionChunkHeader))
	{
		r = newchunk(CHUNK_SIZE);
		r->chunk.next = (struct _RegionChunk*)pool->chunks;
		pool->chunks = r;
	}
	else
	{
		r = newchunk(szbytes + 2 * sizeof(RegionChunkHeader));
		r->chunk.next = (struct _RegionChunk*)pool->jumbo;
		pool->jumbo = r;
	}
	void* p = &(r->data[r->chunk.used]);
	r->chunk.used += szbytes;

	return p;
}
