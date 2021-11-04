/*
 * kernel_extra.c - Project 2 Extra Credit, CMPSC 473
 * Copyright 2021 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <malloc.h>
#include <types.h>
#include <string.h>
#include <printf.h>

/* What is the correct alignment? */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

static size_t HDRSIZE = 8;        // size of header/footer
static size_t DHDRSIZE = 16;      // double the size of header/footer
static size_t CHUNKSIZE = 1 <<12; // 4096 bytes
static char *heap_listp;          // points to prologue block

/* 
 * Helper functions to find block addresses and populate header/footer
 */
static size_t packW (size_t size, char alloc) {
    return size | alloc;
}
static size_t getW(char *p){
    return *(size_t *)p;
}
static void putW(char *p, size_t val){
    *(size_t *)p = val;
}
static size_t getSize(char *p){
    return getW(p) & ~0xf;
}
static size_t getAllocation(char *p) {
    return getW(p) & 0x1;
}
static char * getHeaderAddress(char *p) {
    return (p - HDRSIZE);
}
static char * getFooterAddress(char *p) {
    char * temp = p - HDRSIZE;  // address of footer of previous block
    return (temp + getSize(temp) - HDRSIZE);
}
static char * getNextBlock(char *p) {
    return ( p + getSize(getHeaderAddress(p))); 
}
static char * getPreviousBlock(char *p) {
    char * temp = p - DHDRSIZE; // address of footer of previous block
    return (p - getSize(temp)); 
}

/* 
 * Helper functions to implement memory allocator functions
 */
static void *coalesce(void *ptr);
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *ptr, size_t size);


// Your mm_init(), malloc(), free() code from mm.c here
// You can only use mem_sbrk(), mem_heap_lo(), mem_heap_hi() and
// Project 2's kernel headers provided in 'include' such
// as memset and memcpy.
// No other files from Project 1 are allowed!

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init()
{
    //Create initial empty head
    if ((heap_listp = mem_sbrk(4*HDRSIZE)) == (void *)-1) {
        return false;    
    }
    putW(heap_listp, 0);            //Initial padding for alignment purposes
    putW(heap_listp +   (HDRSIZE), packW(DHDRSIZE, 1));   //Prologue header
    putW(heap_listp + (2*HDRSIZE), packW(DHDRSIZE, 1));   //Prologue footer
    putW(heap_listp + (3*HDRSIZE), packW(0, 1));          //Epilogue header
    heap_listp += (2*HDRSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/HDRSIZE) == NULL) {
        return false;
    }
    return true;
}

/*
 * malloc: returns a pointr to an allocated block payload of at least size bytes
 */
void *malloc(size_t size)
{
    size_t adjsize;         // adjusted block size
    size_t extendsize;      // amount to extend heap if there is no suitable space in heap
    char *ptr;
    // ignore spurious requests
    if (size == 0){
        return NULL;
    }
    // find the adjusted block size including space for header/footer and alignment correction
    adjsize = align(size + HDRSIZE + HDRSIZE);
    // search block list for a suitable free block
    if ((ptr = find_fit(adjsize)) != NULL) {    // block found, allocate
        place(ptr, adjsize);                    // allocate block in found address 
        return ptr;
    } 
    // no block was found, extend heap and allocate block
    extendsize = (adjsize > CHUNKSIZE) ? adjsize : CHUNKSIZE;  // extend heap a min of CHUNKSIZE bytes
    if ((ptr = extend_heap(extendsize/HDRSIZE)) == NULL){
        return NULL;                            // unable to expand heap
    }
    place(ptr, adjsize);                        // allocate block in newly made free space
    return ptr;
}

/*
 * free: frees block located at ptr
 */
void free(void *ptr)
{
    // ignore spurious requests
    if (ptr == NULL) {
        return;
    }
    // update header and footer to indicate block is free
    size_t size = getSize(getHeaderAddress(ptr));
    putW(getHeaderAddress(ptr),packW(size, 0));
    putW(getFooterAddress(ptr),packW(size, 0));
    coalesce(ptr);          // try to coalesce this free block with previous and/or next
    return;
}

/*
 * coalesce: Merges free block with previous and next block if possible
 */
static void *coalesce(void *ptr){
    void *next = getNextBlock(ptr);
    void *prev = getPreviousBlock(ptr);
    size_t prev_allocation = getAllocation(getFooterAddress(prev));
    size_t next_allocation = getAllocation(getHeaderAddress(next));
    size_t size = getSize(getHeaderAddress(ptr));   // size of current block
    /* Check if it is possible to coalesce */
    // Case 1 - previous and next blocks are not free: nothing to do
    if (prev_allocation && next_allocation){
        return ptr;
    }
    // Case 2 - previous block not free, next block is free: expand current block into next
    else if (prev_allocation && !next_allocation) {
        size += getSize(getHeaderAddress(next));
        putW(getFooterAddress(next),packW(size,0));
        putW(getHeaderAddress(ptr), packW(size,0));
    }
    // Case 3 - previous block is free, next block is not: expand previous block into current
    else if (!prev_allocation && next_allocation){
        size += getSize(getHeaderAddress(prev));
        putW(getHeaderAddress(prev), packW(size,0));
        putW(getFooterAddress(ptr), packW(size,0));
        ptr = prev;
    }
    // Case 4 - previous and next blocks are free: expand previous block into next
    else {
        size += getSize(getHeaderAddress(prev)) + getSize(getHeaderAddress(next));
        putW(getHeaderAddress(prev), packW(size,0));
        putW(getFooterAddress(next), packW(size,0));
        ptr = prev;
    }
    return ptr;
}
/*
 * extend_heap: extends heap size by given number of words
 */
static void *extend_heap(size_t words){
    char *ptr;
    size_t size;
    //Adjust size to make sure alignment is correct
    size = (words % 2) ? (words + 1) * HDRSIZE : words * HDRSIZE;
    if ((long) (ptr = mem_sbrk(size)) == -1){
        return NULL;
    }
    //Initialize header/footer in new block and epilogue header
    putW(getHeaderAddress(ptr), packW(size, 0));           // free block header
    putW(getFooterAddress(ptr), packW(size, 0));           // free block footer
    putW(getHeaderAddress(getNextBlock(ptr)), packW(0,1)); // new epilogue header
    // check if there are blocks to coalesce (the previous last block might be free)
    return coalesce(ptr);
}
/*
 * find_fit: loops through all blocks to find a free one that fits requested size heap size
 */
static void *find_fit(size_t size) {
    char *currentp;
    size_t currentsize;

    // start traversing the implicit list starting from the first block (after prologue)
    currentp = getNextBlock(heap_listp);
    while (getSize(getHeaderAddress(currentp)) > 0) {
        currentsize  = getSize(getHeaderAddress(currentp));
        if (currentsize <= 0){  // search reached the epilogue block
            return NULL;
        }
        if (getAllocation(getHeaderAddress(currentp)) == 0) {  // make sure block is free
            if (currentsize >=  size) {
                return currentp;
            }
        }
        currentp = getNextBlock(currentp);
    }
    return NULL;
}
/*
 * place: places block size at the beginning of given free block and split it if this block has space to spare
 */
static void place(void *ptr, size_t size){
    size_t freesize = getSize(getHeaderAddress(ptr));
    // check if the size of the free block given is big enough to split after taking the requested bytes
    if ((freesize - size) >  2*DHDRSIZE) {   // a block min size is 2*16 to fit hdr, ftr and payload
        // split free block into 2 blocks
        void *newptr = ptr + size;
        putW(getHeaderAddress(newptr), packW((freesize - size), 0));
        putW(getFooterAddress(newptr), packW((freesize - size), 0));
        putW(getHeaderAddress(ptr), packW(size, 1));
        putW(getFooterAddress(ptr), packW(size, 1));
    }
    else {
        // take the whole free block
        putW(getHeaderAddress(ptr), packW(freesize, 1));
        putW(getFooterAddress(ptr), packW(freesize, 1));
    }
}



