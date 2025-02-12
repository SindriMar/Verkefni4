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
	if(newstart == (void *)-1) return NULL; // Fix: Proper NULL return on failure
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
	if (_heapStart == NULL) {
		fprintf(stderr, "Heap initialization failed.\n");
		return;
	}
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
	return (n + 15) & ~15;
}

void *my_malloc(uint64_t size)
{
	if (size == 0) return NULL;

	size = roundUp(size) + sizeof(Block);
	if (_firstFreeBlock == NULL) {
		uint8_t *newHeap = allocHeap(_heapStart, _heapSize + HEAP_SIZE);
		if (newHeap == NULL) return NULL; // No more memory available

		_heapSize += HEAP_SIZE;
		
		Block *newBlock = (Block *)(_heapStart + _heapSize - HEAP_SIZE);
		newBlock->size = HEAP_SIZE - sizeof(Block);
		newBlock->next = _firstFreeBlock;
		_firstFreeBlock = newBlock;
	}

	Block *bestFit = NULL, **prevBestFit = NULL;
	Block **prev = &_firstFreeBlock;
	Block *current = _firstFreeBlock;

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

	if (!bestFit) return NULL;

	*prevBestFit = bestFit->next;

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

void my_free(void *address)
{
	if (!address) return;

	Block *block = (Block *)((uint8_t *)address - sizeof(Block));

	Block **prev = &_firstFreeBlock;
	Block *current = _firstFreeBlock;
	while (current && current < block) {
		prev = &current->next;
		current = current->next;
	}

	block->next = current;
	*prev = block;

	if (current && (uint8_t *)block + block->size == (uint8_t *)current) {
		block->size += current->size;
		block->next = current->next;
	}

	if (prev != &_firstFreeBlock) {
		Block *prevBlock = *prev;
		if ((uint8_t *)prevBlock + prevBlock->size == (uint8_t *)block) {
			prevBlock->size += block->size;
			prevBlock->next = block->next;
		}
	}
}
