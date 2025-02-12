#define USE_REAL_SBRK 1 // Breyta yfir í 1 fyrir skil!
#pragma GCC diagnostic ignored "-Wunused-function"

#if USE_REAL_SBRK
#define _GNU_SOURCE

#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h> 
#include <assert.h>
#include <unistd.h>

#include "malloc.h"

uint8_t *allocHeap(uint8_t *currentHeap, uint64_t size)
{                
        static uint64_t heapSize = 0;
        if( currentHeap == NULL ) {
                uint8_t *newHeap  = sbrk(size);
                if(newHeap)
                        heapSize = size;
                return newHeap;
        }
	uint8_t *newstart = sbrk(size - heapSize);
	if(newstart == NULL) return NULL;
	heapSize += size;
	return currentHeap;
}
#else
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include "malloc.h"

uint8_t *allocHeap(uint8_t *currentHeap, uint64_t size)
{
        static uint64_t heapSize = 0;
        if( currentHeap == NULL ) {
                uint8_t *newHeap  = malloc(10*size);
                if(newHeap)
                        heapSize = 10*size;
                return newHeap;
        }
	if(size <= heapSize) return currentHeap;
        return NULL;
}
#endif

uint8_t *_heapStart = NULL;
uint64_t _heapSize = 0;
Block *_firstFreeBlock;

void initAllocator()
{
        _heapStart = allocHeap(NULL, HEAP_SIZE);
	_heapSize = HEAP_SIZE;

	_firstFreeBlock = (Block *)_heapStart;
	_firstFreeBlock->size = _heapSize - sizeof(Block);
	_firstFreeBlock->next = NULL;
}

static Block *_getNextBlockBySize(const Block *current)
{
	return (Block *)((uint8_t *)current + current->size);
}

void dumpAllocator()
{
	// See lab tutorial
}

uint64_t roundUp(uint64_t n)
{
	return (n + 15) & ~15; //fancy trick I found on the internet for rounding up to the nearest multiple of 16.
}

static void *allocate_block(Block **update_next, Block *block, uint64_t new_size)
{
	(void)update_next;
	(void)block;
	(void)new_size;
	return NULL;
}

void *my_malloc(uint64_t size)
{
	if (size == 0) return NULL;
	size = roundUp(size) + sizeof(Block);

	Block *bestFit = NULL, **prevBestFit = NULL;
	Block **prev = &_firstFreeBlock;
	Block *current = _firstFreeBlock;

try_alloc:
	while (current) {
		if (current->size >= size) {
			if (!bestFit || current->size < bestFit->size) {
				bestFit = current;
				prevBestFit = prev;
			}
		}
		prev = &current->next;
		current = current->next;
	}

	// If no suitable block found, try to expand heap
	if (!bestFit) {
		uint8_t *newHeap = allocHeap(_heapStart, _heapSize + HEAP_SIZE);
		if (!newHeap) return NULL; // If heap expansion fails, return NULL

		Block *newBlock = (Block *)(_heapStart + _heapSize);
		newBlock->size = HEAP_SIZE;
		newBlock->next = _firstFreeBlock;
		_firstFreeBlock = newBlock;
		_heapSize += HEAP_SIZE;

		// Restart search for best-fit block
		bestFit = NULL;
		prevBestFit = NULL;
		prev = &_firstFreeBlock;
		current = _firstFreeBlock;
		goto try_alloc;
	}

	// Remove the block from free list
	*prevBestFit = bestFit->next;

	// Split block if there's enough space for a new block
	if (bestFit->size > size + sizeof(Block)) {
		Block *newBlock = (Block *)((uint8_t *)bestFit + size);
		newBlock->size = bestFit->size - size;
		newBlock->next = _firstFreeBlock;
		_firstFreeBlock = newBlock;
		bestFit->size = size;
	}

	bestFit->next = (Block *)0xfeedcafefeedcafe;
	return bestFit->data;
}

static void merge_blocks(Block *block1, Block *block2)
{
	if ((uint8_t *)block1 + block1->size == (uint8_t *)block2) {
		block1->size += block2->size;
		block1->next = block2->next;
	}
}

void my_free(void *address)
{
	if (!address) return;

	Block *block = (Block *)((uint8_t *)address - sizeof(Block));
	block->next = _firstFreeBlock;
	_firstFreeBlock = block;

	Block *current = _firstFreeBlock;
	while (current && current->next) {
		if ((uint8_t *)current + current->size == (uint8_t *)current->next) {
			current->size += current->next->size;
			current->next = current->next->next;
		}
		current = current->next;
	}
}
