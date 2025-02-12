#include <stddef.h>
#include <stdio.h> 
#include <assert.h>
#include <unistd.h>
#include "malloc.h"

// Global variables
uint8_t *_heapStart = NULL;
uint64_t _heapSize = 0;
Block *_firstFreeBlock = NULL;

// Initializes the allocator with one free block spanning the initial heap.
void initAllocator()
{
    _heapStart = allocHeap(NULL, HEAP_SIZE);
    assert(_heapStart != NULL);
    _heapSize = HEAP_SIZE;

    _firstFreeBlock = (Block *)_heapStart;
    _firstFreeBlock->size = _heapSize - sizeof(Block);
    _firstFreeBlock->next = NULL;
}

// Rounds up n to the next multiple of 16.
uint64_t roundUp(uint64_t n)
{
    return (n + 15) & ~15;
}

/*
  insertFreeBlock()
  
  Inserts the given free block into the free-list (which is maintained in
  sorted order by address) and then merges it with its neighbors if they
  are contiguous.
*/
static void insertFreeBlock(Block *block)
{
    // Insert block into the free list in sorted order.
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

    // Merge with the next block if adjacent.
    if (block->next && ((uint8_t*)block + block->size == (uint8_t*)block->next)) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
    
    // Merge with the previous block if adjacent.
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

/*
  my_malloc()
  
  Tries to find a free block that fits the requested user size (after rounding
  and adding the header size). If no free block is large enough, the heap is extended.
  On allocation the chosen free block is removed from the free list; if it is large
  enough to split then it is split and the remainder is reinserted.
  
  Note: We do not change the provided allocHeap() code.
*/
void *my_malloc(uint64_t userSize)
{
    if (userSize == 0)
        return NULL;
    
    // Compute total size needed: round up the payload then add the header.
    uint64_t totalSize = roundUp(userSize) + sizeof(Block);

    Block *bestFit = NULL;
    Block **prevBestFit = NULL;
    Block **prev = &_firstFreeBlock;
    Block *current = _firstFreeBlock;

    // Search for a free block that is large enough.
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

    // If no free block is large enough, try extending the heap.
    if (!bestFit) {
        uint64_t oldHeapSize = _heapSize;
        uint64_t newHeapSize = _heapSize + HEAP_SIZE; // Extend by HEAP_SIZE bytes.
        uint8_t *res = allocHeap(_heapStart, newHeapSize);
        if (res == NULL)
            return NULL;
        // Create a free block covering the newly allocated area.
        Block *newBlock = (Block *)(_heapStart + oldHeapSize);
        newBlock->size = newHeapSize - oldHeapSize;
        newBlock->next = NULL;
        insertFreeBlock(newBlock);
        _heapSize = newHeapSize;
        // Retry allocation.
        return my_malloc(userSize);
    }

    // Remove the chosen block from the free list.
    *prevBestFit = bestFit->next;

    // If the block is large enough, split it into an allocated part and a free part.
    if (bestFit->size >= totalSize + sizeof(Block) + 16) {
        Block *newBlock = (Block *)((uint8_t *)bestFit + totalSize);
        newBlock->size = bestFit->size - totalSize;
        newBlock->next = NULL;
        bestFit->size = totalSize;
        insertFreeBlock(newBlock);
    }

    // Mark the block as allocated.
    bestFit->next = (Block *)0xfeedcafefeedcafe;
    return bestFit->data;
}

/*
  my_free()
  
  Frees the block by converting the given data pointer into its header pointer
  and then inserting it (and merging with any adjacent free blocks) into the free list.
*/
void my_free(void *address)
{
    if (!address)
        return;
    Block *block = (Block *)((uint8_t *)address - sizeof(Block));
    block->next = NULL;
    insertFreeBlock(block);
}
