#ifndef _mem_h_
#define _mem_h_
#include <stddef.h>

typedef unsigned char byte_t;
typedef void *(*alloc_pfn)(size_t);

typedef struct dynpool_block dynpool_block_t;
typedef struct dynpool *dynpool_t;
typedef struct dynpool_chunk {
	union {
		dynpool_block_t *parent;
		struct dynpool_chunk *nextfree;
	};
	byte_t data[];
} dynpool_chunk_t;

struct dynpool_block {
#ifdef DEBUG
	dynpool_t dbg_parent;
#endif
	dynpool_block_t *next, *nextfree, *lastfree;
	size_t nchunks, freeid, elemsz, nalloced;
	dynpool_chunk_t *free;
	byte_t data[];
};

struct dynpool {
	size_t elemsz;
	alloc_pfn allocer;
	dynpool_block_t *head, *free;
	dynpool_block_t first;
};

dynpool_t dynpool_init(size_t initial_chunks, size_t elemsz, alloc_pfn allocer);
void dynpool_deinit(dynpool_t self);
void *dynpool_alloc(dynpool_t self);
void dynpool_free(dynpool_t self, void *ptr);
int dynpool_empty(const dynpool_t self);
void dynpool_fast_clear(dynpool_t self);
size_t dynpool_num_chunks(const dynpool_t self);

typedef struct fixedpool_chunk {
	struct fixedpool_chunk *next;
} fixedpool_chunk_t;

typedef struct fixedpool {
	size_t nchunks, freeid, elemsz;
	fixedpool_chunk_t *free;
	byte_t data[];
} fixedpool_t;

void fixedpool_init(fixedpool_t *self, size_t elemsz, size_t buflen);
void fixedpool_fast_clear(fixedpool_t *self);
void *fixedpool_alloc(fixedpool_t *self);
void fixedpool_free(fixedpool_t *self, void *blk);
int fixedpool_empty(const fixedpool_t *self);

#define ARENA_SIZE_LOW_MEM (1024*1024*12)
#ifndef CNM_LOW_MEM
#define ARENA_SIZE (1024*1024*48)
#else
#define ARENA_SIZE ARENA_SIZE_LOW_MEM 
#endif

void arena_init(const char *base_zone_name);
void arena_deinit(void);
void *arena_alloc(size_t size);
void arena_popfree(void *blk);
void arena_push_zone(const char *zone_name);
void arena_pop_zone(const char *expected_zone);
void *arena_global_alloc(size_t size);
void arena_global_popfree(void *blk);
size_t arena_used_mem(void);

#endif

