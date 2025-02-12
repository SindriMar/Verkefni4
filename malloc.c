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
    if (currentHeap == NULL) {
        uint8_t *newHeap = sbrk(size);
        if (newHeap) heapSize = size;
        return newHeap;
    }
    uint8_t *newstart = sbrk(size - heapSize);
    if (newstart == NULL) return NULL;
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
    if (currentHeap == NULL) {
        uint8_t *newHeap = malloc(10 * size);
        if (newHeap) heapSize = 10 * size;
        return newHeap;
    }
    if (size <= heapSize) return currentHeap;
    return NULL;
}
#endif

/*------------------------------------------------------------------
  Forward declarations to avoid implicit declaration warnings.
  (Note: You are not allowed to change the above allocHeap code.)
------------------------------------------------------------------*/
void dumpAllocator(void);

/*------------------------------------------------------------------
  Global variables for the allocator
------------------------------------------------------------------*/
uint8_t *_heapStart = NULL;
uint64_t _heapSize = 0;
Block *_firstFreeBlock = NULL;

/*------------------------------------------------------------------
  Initializes the allocator. It allocates an initial heap region and
  sets up one large free block.
------------------------------------------------------------------*/
void initAllocator()
{
    _heapStart = allocHeap(NULL, HEAP_SIZE);
    assert(_heapStart != NULL);
    _heapSize = HEAP_SIZE;

    _firstFreeBlock = (Block *)_heapStart;
    _firstFreeBlock->size = _heapSize - sizeof(Block);
    _firstFreeBlock->next = NULL;
}

/*------------------------------------------------------------------
  Rounds up n to the nearest multiple of 16.
------------------------------------------------------------------*/
uint64_t roundUp(uint64_t n)
{
    return (n + 15) & ~15;
}

/*------------------------------------------------------------------
  Inserts a free block into the free list (which is kept in sorted
  order by address) and merges with adjacent free blocks if possible.
------------------------------------------------------------------*/
static void insertFreeBlock(Block *block)
{
    if (_firstFreeBlock == NULL || block < _firstFreeBlock) {
        block->next = _firstFreeBlock;
        _firstFreeBlock = block;
    } else {
        Block *curr = _firstFreeBlock;
        while (curr->next && curr->next < block)
            curr = curr->next;
        block->next = curr->next;
        curr->next = block;
    }

    // Merge with next block if adjacent.
    if (block->next && ((uint8_t*)block + block->size == (uint8_t*)block->next)) {
        block->size += block->next->size;
        block->next = block->next->next;
    }

    // Merge with previous block if adjacent.
    if (_firstFreeBlock != block) {
        Block *prev = _firstFreeBlock;
        while (prev && prev->next != block)
            prev = prev->next;
        if (prev && ((uint8_t*)prev + prev->size == (uint8_t*)block)) {
            prev->size += block->size;
            prev->next = block->next;
        }
    }
}

/*------------------------------------------------------------------
  my_malloc: Allocates memory using a best-fit strategy. If no
  free block is large enough, it extends the heap.
------------------------------------------------------------------*/
void *my_malloc(uint64_t size)
{
    if (size == 0)
        return NULL;

    uint64_t totalSize = roundUp(size) + sizeof(Block);
    Block *bestFit = NULL;
    Block **prevBestFit = NULL;
    Block **prev = &_firstFreeBlock;
    Block *current = _firstFreeBlock;

    while (current) {
        if (current->size >= totalSize) {
            if (!bestFit || current->size < bestFit->size) {
                bestFit = current;
                prevBestFit = prev;
            }
        }
        prev = &current->next;
        current = current->next;
    }

    if (!bestFit) {
        // Extend the heap.
        uint64_t oldHeapSize = _heapSize;
        uint64_t newHeapSize = _heapSize + HEAP_SIZE; // Extend by HEAP_SIZE bytes.
        uint8_t *res = allocHeap(_heapStart, newHeapSize);
        if (res == NULL)
            return NULL;
        // Create a new free block in the extended area.
        Block *newBlock = (Block *)(_heapStart + oldHeapSize);
        newBlock->size = newHeapSize - oldHeapSize;
        newBlock->next = NULL;
        insertFreeBlock(newBlock);
        _heapSize = newHeapSize;
        // Retry allocation.
        return my_malloc(size);
    }

    // Remove bestFit from the free list.
    *prevBestFit = bestFit->next;

    // Split the block if there is room for a new free block.
    if (bestFit->size >= totalSize + sizeof(Block) + 16) {
        Block *newBlock = (Block *)((uint8_t *)bestFit + totalSize);
        newBlock->size = bestFit->size - totalSize;
        newBlock->next = NULL;
        bestFit->size = totalSize;
        insertFreeBlock(newBlock);
    }

    bestFit->next = (Block *)0xfeedcafefeedcafe; // Mark as allocated.
    return bestFit->data;
}

/*------------------------------------------------------------------
  my_free: Frees a block by inserting it into the free list in sorted
  order (merging with adjacent free blocks as needed).
------------------------------------------------------------------*/
void my_free(void *address)
{
    if (!address)
        return;
    Block *block = (Block *)((uint8_t *)address - sizeof(Block));
    block->next = NULL;
    insertFreeBlock(block);
}

/*------------------------------------------------------------------
  dumpAllocator: Dummy implementation.
  
  (The lab tests expect this function to exist even if it doesn't
  output anything. You may expand this function for debugging.)
------------------------------------------------------------------*/
void dumpAllocator(void)
{
    // Dummy implementation.
    // For example, you might print the list of free blocks:
    /*
    Block *curr = _firstFreeBlock;
    printf("Free List:\n");
    while (curr) {
        printf("Block at %p, size %lu\n", (void*)curr, curr->size);
        curr = curr->next;
    }
    */
}
