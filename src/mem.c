#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mem.h"

#define Console_Print printf

#define DBGARENA 0

dynpool_t dynpool_init(size_t initial_chunks, size_t elemsz, alloc_pfn allocer) {
	elemsz = ((elemsz - 1) / sizeof(dynpool_chunk_t) + 2) * sizeof(dynpool_chunk_t);
	dynpool_t self = allocer(sizeof(*self) + sizeof(dynpool_block_t) + initial_chunks * elemsz);
	*self = (struct dynpool){
		.elemsz = elemsz,
		.allocer = allocer,
		.head = &self->first,
		.free = &self->first,
		.first = (dynpool_block_t){
#ifdef DEBUG
			.dbg_parent = self,
#endif
			.elemsz = elemsz,
			.nchunks = initial_chunks,
			.next = NULL,
			.nextfree = NULL,
			.lastfree = NULL,
			.free = NULL,
			.freeid = 0,
			.nalloced = 0,
		},
	};

	return self;
}
void dynpool_deinit(dynpool_t self) {
	// Now a no-op
	//dynpool_block_t *blk = self->head;
	//while (blk) {
	//	dynpool_block_t *next = blk->next;
	//	if (blk != &self->first) free(blk);
	//	blk = next;
	//}
	//free(self);
}
static void remove_free_block(dynpool_t self, dynpool_block_t *blk) {
	if (blk->lastfree) blk->lastfree->nextfree = blk->nextfree;
	else self->free = blk->nextfree;
	if (blk->nextfree) blk->nextfree->lastfree = blk->lastfree;
}
void *dynpool_alloc(dynpool_t self) {
	if (!self->free) {
		dynpool_block_t *blk = self->allocer(sizeof(*blk) + self->head->nchunks * self->elemsz);
		*blk = (dynpool_block_t){
#ifdef DEBUG
			.dbg_parent = self,
#endif
			.elemsz = self->elemsz,
			.nchunks = self->head->nchunks,
			.next = self->head,
			.nextfree = NULL,
			.lastfree = NULL,
			.free = NULL,
			.freeid = 0,
			.nalloced = 0,
		};
		self->head = blk;
		self->free = blk;
	}

	dynpool_block_t *blk = self->free;
	dynpool_chunk_t *chunk;
	if (blk->freeid != blk->nchunks) {
		chunk = (dynpool_chunk_t *)(blk->data + (blk->freeid++ * blk->elemsz));
	} else {
		chunk = blk->free;
		blk->free = chunk->nextfree;
	}
	chunk->parent = blk;
	if (++blk->nalloced == blk->nchunks) remove_free_block(self, blk);
	return chunk->data;
}
void dynpool_free(dynpool_t self, void *ptr) {
	if (!ptr) {
#ifdef DEBUG
		Console_Print("Double free on dynpool!!!!\n");
#endif
		return;
	}
	dynpool_chunk_t *chunk = (dynpool_chunk_t *)((uintptr_t)ptr - offsetof(dynpool_chunk_t, data));
	dynpool_block_t *blk = chunk->parent;
#ifdef DEBUG
	assert(self == blk->dbg_parent);
#endif
	if (blk->nalloced-- == blk->nchunks) {
		blk->lastfree = NULL;
		blk->nextfree = self->free;
		if (blk->nextfree) blk->nextfree->lastfree = blk;
		self->free = blk;
	}

	chunk->nextfree = blk->free;
	blk->free = chunk;
}
int dynpool_empty(const dynpool_t self) {
	size_t free_blocks = 0, nblocks = 0;
	
	const dynpool_block_t *blk = self->head;
	while (blk) {
		nblocks++;
		blk = blk->next;
	}

	blk = self->free;
	while (blk) {
		if (blk->nalloced != 0) return 0;
		free_blocks++;
		blk = blk->nextfree;
	}

	return free_blocks == nblocks;
}
void dynpool_fast_clear(dynpool_t self) {
	dynpool_block_t *blk = self->head;
	self->free = NULL;
	while (blk) {
		blk->nalloced = 0;
		blk->free = NULL;
		blk->freeid = 0;
		
		blk->lastfree = NULL;
		blk->nextfree = self->free;
		if (blk->nextfree) blk->nextfree->lastfree = blk;
		self->free = blk;

		blk = blk->next;
	}

	// DEBUG: remove this
	assert(dynpool_empty(self));
}
size_t dynpool_num_chunks(const dynpool_t self) {
	size_t nchunks = 0;
	dynpool_block_t *blk = self->head;
	while (blk) {
		nchunks += blk->nalloced;
		blk = blk->next;
	}
	return nchunks;
}

void fixedpool_init(fixedpool_t *self, size_t elemsz, size_t buflen) {
	if (elemsz < sizeof(fixedpool_chunk_t)) elemsz = sizeof(fixedpool_chunk_t);
	elemsz = ((elemsz - 1) / sizeof(fixedpool_chunk_t) + 1) * sizeof(fixedpool_chunk_t);
	self->nchunks = (buflen - sizeof(*self)) / elemsz;
	self->free = NULL;
	self->freeid = 0;
	self->elemsz = elemsz;
}
void fixedpool_fast_clear(fixedpool_t *self) {
	self->freeid = 0;
	self->free = NULL;
}
void *fixedpool_alloc(fixedpool_t *self) {
	if (self->freeid != self->nchunks) {
		return self->data + self->elemsz*(self->freeid++);
	}
	if (!self->free) return NULL;
	void *ptr = self->free;
	self->free = self->free->next;
	return ptr;
}
void fixedpool_free(fixedpool_t *self, void *blk) {
	if (!blk) {
#ifdef DEBUG
		Console_Print("Double free on fixedpool!!!!\n");
#endif
		return;
	}
	((fixedpool_chunk_t *)blk)->next = self->free;
	self->free = blk;
}
int fixedpool_empty(const fixedpool_t *self) {
	if (self->freeid == 0) return 1;
	size_t nfree = 0;
	fixedpool_chunk_t *chunk = self->free;
	while (chunk) {
		nfree++;
		chunk = chunk->next;
	}
	return nfree == self->nchunks;
}

#define ARENA_STACK_SIZE 64
static byte_t *_stack[ARENA_STACK_SIZE];
static const char *_stack_dbgnames[ARENA_STACK_SIZE];
static int _stack_top;
static _Alignas(_Alignof(size_t)) byte_t _arena[ARENA_SIZE];
static byte_t *_arena_head;
static byte_t *_arena_tail;

void arena_init(const char *base_zone_name) {
	_stack_top = 0;
	_arena_head = _arena;
	_arena_tail = _arena + ARENA_SIZE;
	_stack[_stack_top] = _arena_head;
	_stack_dbgnames[_stack_top] = base_zone_name;
}
void arena_deinit(void) {
	assert(_arena_head >= _stack[0]);
}
void *arena_alloc(size_t size) {
	size = ((size - 1) / sizeof(size_t) + 1) * sizeof(size_t);
	void *ptr = _arena_head;
	_arena_head += size;
	*(size_t *)_arena_head = size;
	_arena_head += sizeof(size_t);
	if (_arena_head > _arena_tail) {
		// Sick hack here.
		_arena_head = _arena;
		Console_Print("OUT OF MEM!!!!\n");
		Console_Print("Found in zone %s\n", _stack_dbgnames[_stack_top]);
		exit(-1);
	}
	assert(_arena_head <= _arena_tail);
	return ptr;
}
void arena_popfree(void *blk) {
	_arena_head -= sizeof(size_t);
	size_t size = *(size_t *)_arena_head;
	_arena_head -= size;
	assert(_arena_head == blk);
}
void arena_push_zone(const char *dbgname) {
#if defined(DEBUG) && DBGARENA == 1
	Console_Print("push zone %s\n", dbgname);
#endif
	assert(_stack_top + 1 < ARENA_STACK_SIZE);
	_stack[++_stack_top] = _arena_head;
	_stack_dbgnames[_stack_top] = dbgname;
}
void arena_pop_zone(const char *expected_zone) {
#if defined(DEBUG) && DBGARENA == 1
	Console_Print("pop zone %s\n", _stack_dbgnames[_stack_top]);
#endif
	if (expected_zone && strcmp(expected_zone, _stack_dbgnames[_stack_top]) != 0) {
		Console_Print("ERROR: Expected arena stack: \"%s\"\n", expected_zone);
		Console_Print("but got stack name \"%s\" instead!\n", _stack_dbgnames[_stack_top]);
		assert(0);
	}
	assert(_stack_top > 0);
	assert(_arena_head >= _stack[_stack_top]);
	_arena_head = _stack[_stack_top--];
}
void *arena_global_alloc(size_t size) {
	size = ((size - 1) / sizeof(size_t) + 2) * sizeof(size_t);
	_arena_tail -= size;
	*(size_t *)_arena_tail = size;
	if (_arena_head > _arena_tail) {
		// Sick hack here.
		_arena_head = _arena;
		Console_Print("OUT OF MEM!!!!\n");
		Console_Print("Found in zone %s\n", _stack_dbgnames[_stack_top]);
		exit(-1);
	}
	assert(_arena_head <= _arena_tail);
	return _arena_tail + sizeof(size_t);
}
void arena_global_popfree(void *blk) {
	assert(_arena_tail + sizeof(size_t) == blk);
	_arena_tail += *(size_t *)_arena_tail;
	assert(_arena_tail <= _arena + ARENA_SIZE);
}
size_t arena_used_mem(void) {
	return ARENA_SIZE - (_arena_tail - _arena_head);
}

