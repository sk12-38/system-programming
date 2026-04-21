/*
 * mm.c - A segregated free list based dynamic memory allocator
 * 
 * This allocator implements dynamic memory management using a segregated free list.
 * It maintains 20 separate free lists, each handling block sizes in exponentially increasing ranges.
 * 
 * Key design choices:
 * 
 * - Alignment: All payload pointers are 8-byte aligned.
 * - Block format: Each block includes a header and footer (4 bytes each).
 *   Free blocks include 16 bytes of payload metadata (prev, next).
 *   The minimum block size is 32 bytes.
 * 
 * - Free lists: Segregated free lists are implemented with explicit pointers.
 *   Free blocks are inserted in sorted order by size to support first-fit search.
 * 
 * - Allocation: The allocator tries to find a fitting block via `find_fit_block`.
 *   If not found, the heap is expanded by 1.5x the request size.
 *   Splitting occurs based on the leftover size (three strategies depending on size gap).
 * 
 * - Coalescing: Adjacent free blocks are merged eagerly upon free or heap extension.
 *   A 3-way merge is implemented if both neighbors are free.
 * 
 * - Reallocation: In-place reallocation is attempted first if space permits.
 *   Otherwise, a new block is allocated and contents are copied.
 *   If the next block is free and combined size fits the new size, the block is expanded in-place.
 * 
 * This design balances space utilization and throughput, aiming for minimal fragmentation
 * and efficient allocation via segregated free lists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* 1. Macro for alignmnent */
#define ALIGNMENT 8 // Alignment size
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // Rounds up to the nearest multiple of ALIGNMENT

/* 2. Macro for size */
#define WSIZE 4                     // Size of word(header/footer)
#define OVERHEAD 8                  // Size of header + footer
#define DSIZE 8                     // Size of double word size(block)
#define PTR_SIZE sizeof(void *)     // Size of a pointer (8B on 64-bit)
#define MAX_FREELISTS 20            // The number of segregated free list
#define MIN_BLOCK_SIZE 32           // Minimum size that free block can take ALIGH(header(4) + padding(4) + prev(8) + next(8) + footer(4)) = 32
#define INITIAL_HEAP_SIZE  (1<<11)  // Initial heap size is 2^12 = 4096 bytes(4KB)

/* 3. Macro for block format */
#define PACK(size, alloc)  ((size) | (alloc))   // Pack a size and allocated bit into a word
#define GET(p)       (*(unsigned int *)(p))     // Read a word at address p
#define PUT(p, val)  (*(unsigned int *)(p) = (val)) // Write a word val at address p
#define GET_SIZE(p)  (GET(p) & ~0x7)            // Extract block size from header/footer
#define GET_ALLOC(p) (GET(p) & 0x1)             // Extract allocation bit from header/footer

/* 4. Macro for pointer operations */
/* bp points the address of block payload */
#define HEADER_PTR(bp)       ((char *)(bp) - WSIZE) // Return address of header given block pointer
#define FOOTER_PTR(bp)       ((char *)(bp) + GET_SIZE(HEADER_PTR(bp)) - DSIZE)  // Return address of footer given block pointer
#define NEXT_BLOCK_PTR(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))   // Return pointer to next block
#define PREV_BLOCK_PTR(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))   // Return pointer to previous block

/* 5. Macro for free linked list pointer */
#define PREV_NODE(bp) (*(char **)((bp)))           // Access previous free block pointer
#define NEXT_NODE(bp) (*(char **)((bp) + PTR_SIZE))   // Access next free block pointer

static void **segregatedFreeLists;
/*  
    segregatedFreeLists[0]: 1-32 bytes
    segregatedFreeLists[1]: 33-64 bytes
    segregatedFreeLists[2]: 65-128 bytes
    ...
*/

team_t team = {
    "20200959",  
    "Seokhee Lee",  
    "lshlkh1234@naver.com",  
};

static void *expand_heap(size_t words);
static void *allocate_block(void *bp, size_t alignedSize);
static void *coalescing_blocks(void *bp);
static void insert_into_free_list(void* bp);
static void remove_from_free_list(void* bp);
void *find_fit_block(size_t size);
void print_heap_layout(void* heap_start, void* heap_end);

int mm_init(void) {
    void *tempFreeLists;

    /* 1. Segregated list의 head와 metadata를 저장할 공간을 확보 */
    size_t init_size = (MAX_FREELISTS * PTR_SIZE) + (4 * WSIZE); // free list + padding + prologue + epilogue
    size_t aligned_init_size = ALIGN(init_size);

    if ((tempFreeLists = mem_sbrk(aligned_init_size)) == (void *)-1)
        return -1;

    /* 2. 할당받은 주소의 시작을 segregated list를 가리키는 변수에 할당 */
    segregatedFreeLists = (void **)tempFreeLists;

    /* 3. 초기화: free list 포인터 null로 설정 */
    for (int i = 0; i < MAX_FREELISTS; i++) {
        segregatedFreeLists[i] = NULL;
    }

    /* 5. 메타데이터 영역 설정 (4개의 word): padding, prologue header/footer, epilogue header */
    char *meta_base = (char *)tempFreeLists + MAX_FREELISTS * PTR_SIZE;

    PUT(meta_base, 0);                              // padding
    PUT(meta_base + WSIZE, PACK(DSIZE, 1));         // prologue header
    PUT(meta_base + 2 * WSIZE, PACK(DSIZE, 1));     // prologue footer
    PUT(meta_base + 3 * WSIZE, PACK(0, 1));         // epilogue header

    /* 6. Segregated list와 metadata를 저장한 후, 여유 공간을 할당하기 위해 heap을 expand해준다. */
    if (expand_heap(INITIAL_HEAP_SIZE) == NULL) {
        return -1;
    }

    return 0;
}

void *mm_malloc(size_t size) {
    if (size == 0) return NULL;

    size_t alignedSize = ALIGN(size + OVERHEAD);
    if (alignedSize < MIN_BLOCK_SIZE)
        alignedSize = MIN_BLOCK_SIZE;
    void* bp = find_fit_block(alignedSize);
    
    /* find_fit_block()이 NULL을 return: 충분한 크기의 free block이 heap에 없다. */
    if (bp == NULL) {
        if ((bp = expand_heap(alignedSize)) == NULL) return NULL;
        bp = find_fit_block(alignedSize);
    }
    
    /* 찾은 block을 요청한 size에 맞게 allocate */
    bp = allocate_block(bp, alignedSize);
    return bp;
}

static void *expand_heap(size_t size) {
    char *bp;

    // 1. 8바이트 정렬 적용
    // size = ALIGN(size);
    size = ALIGN(size + (size / 2)); // 요청의 150% 확보

    // 2. 힙 공간 확장
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    // 3. 새 블록 header, footer, epilogue 설정 (free 상태)
    PUT(HEADER_PTR(bp), PACK(size, 0));
    PUT(FOOTER_PTR(bp), PACK(size, 0));
    PUT(HEADER_PTR(NEXT_BLOCK_PTR(bp)), PACK(0, 1));

    // 4. Free list에 삽입 후 병합
    insert_into_free_list(bp);
    return coalescing_blocks(bp);
}

void mm_free(void *bp) {
    /* 0. bp가 NULL이면 아무 동작도 하지 않는다. */
    if (bp == NULL) return;

    /* 1. 해제할 block의 크기를 읽어와 할당되지 않음을 마킹한다. */
    size_t size = GET_SIZE(HEADER_PTR(bp));
    PUT(HEADER_PTR(bp), PACK(size, 0));
    PUT(FOOTER_PTR(bp), PACK(size, 0));

    /* 2. 해제한 block을 free_list에 돌려준다. */
    insert_into_free_list(bp);

    /* 3. 해제한 block의 이전 혹은 다음 block 또한 free인 경우에는 coalescing */
    coalescing_blocks(bp);
}

static void insert_into_free_list(void* bp) {
    int listIndex = 0;
    size_t blockSize = GET_SIZE(HEADER_PTR(bp));
    size_t currentSize = MIN_BLOCK_SIZE;    // 최소 사이즈는 MIN_BLOCK_SIZE
    
    /* 1. 블록의 크기에 따라 어느 free list에 들어갈지 index 결정 */
    while (listIndex < MAX_FREELISTS - 1 && blockSize > currentSize) {
        listIndex++;
        currentSize *= 2;
    }
    void* nextBlock = segregatedFreeLists[listIndex];
    void* prevBlock = NULL;

    /* 2. 동일 리스트 내에서 새로운 block이 들어갈 위치 탐색, 이 때, First-fit이 가장 optimal하도록 크기 오름차순으로 정렬 */
    while (nextBlock != NULL && GET_SIZE(HEADER_PTR(bp)) > GET_SIZE(HEADER_PTR(nextBlock))) {
        prevBlock = nextBlock;
        nextBlock = NEXT_NODE(nextBlock);
    }

    /* 3. block을 탐색한 list의 위치에 추가 */
    if (prevBlock == NULL && nextBlock != NULL) {   /* list의 맨 앞에 추가 */
        segregatedFreeLists[listIndex] = bp;
        PREV_NODE(bp) = NULL;
        NEXT_NODE(bp) = nextBlock;
        PREV_NODE(nextBlock) = bp;
    } else if (prevBlock != NULL && nextBlock == NULL) {    /* list의 맨 끝에 추가 */
        NEXT_NODE(prevBlock) = bp;
        PREV_NODE(bp) = prevBlock;
        NEXT_NODE(bp) = NULL;
    } else if (prevBlock != NULL && nextBlock != NULL) {    /* list의 중간에 추가 */
        PREV_NODE(nextBlock) = bp;
        NEXT_NODE(prevBlock) = bp;
        PREV_NODE(bp) = prevBlock;
        NEXT_NODE(bp) = nextBlock;
    } else if (prevBlock == NULL && nextBlock == NULL) {    /* list에 원소가 하나도 없는 경우 */
        segregatedFreeLists[listIndex] = bp;
        PREV_NODE(bp) = NULL;
        NEXT_NODE(bp) = NULL;
    }
}

static void remove_from_free_list(void* bp) {
    int listIndex = 0;
    size_t blockSize = GET_SIZE(HEADER_PTR(bp));
    size_t currentSize = MIN_BLOCK_SIZE;    // 최소 사이즈는 MIN_BLOCK_SIZE

    void* next = NEXT_NODE(bp);
    void* prev = PREV_NODE(bp);

    /* 1. 블록의 크기에 따라 어느 free list에 들어갈지 index 결정 */
    while (listIndex < MAX_FREELISTS - 1 && blockSize > currentSize) {
        listIndex++;
        currentSize *= 2;
    }
    /* 2. bp의 위치에 따라 list 조정 */
    if (prev == NULL && next == NULL) { // prev도 없고 next도 없음: 리스트에 bp만 있는 경우
        segregatedFreeLists[listIndex] = NULL;
        return;
    }
    else if (prev == NULL && next != NULL) { // prev가 없고 next만 있음: bp는 head임
        segregatedFreeLists[listIndex] = next;
        PREV_NODE(next) = NULL;
        return;
    }
    else if (prev != NULL && next == NULL) { // prev만 있고 next 없음: bp는 tail임
        NEXT_NODE(prev) = NULL;
        return;
    }
    else {  // prev와 next 둘 다 있음: 중간 노드
        NEXT_NODE(prev) = next;
        PREV_NODE(next) = prev;
    }
}

static void *coalescing_blocks(void *bp) {  /* 이 함수가 호출되기 전에 항상 insert_into_free_list가 호출되므로 bp는 항상 free_list에 insert 되어있음이 보장됨 */
    void* prev_bp = PREV_BLOCK_PTR(bp);
    void* next_bp = NEXT_BLOCK_PTR(bp);
    int prev_alloc = GET_ALLOC(HEADER_PTR(prev_bp));
    int next_alloc = GET_ALLOC(HEADER_PTR(next_bp));
    size_t size = GET_SIZE(HEADER_PTR(bp));

    if (!prev_alloc && !next_alloc) {   // 1. prev, next 모두 free: 3-way 병합
        remove_from_free_list(prev_bp);
        remove_from_free_list(bp);
        remove_from_free_list(next_bp);

        size += GET_SIZE(HEADER_PTR(prev_bp)) + GET_SIZE(HEADER_PTR(next_bp));
        PUT(HEADER_PTR(prev_bp), PACK(size, 0));
        PUT(FOOTER_PTR(next_bp), PACK(size, 0));
        bp = prev_bp;

    } else if (!prev_alloc && next_alloc) { // 2. prev만 free: 2-way 병합
        remove_from_free_list(prev_bp);
        remove_from_free_list(bp);

        size += GET_SIZE(HEADER_PTR(prev_bp));
        PUT(HEADER_PTR(prev_bp), PACK(size, 0));
        PUT(FOOTER_PTR(bp), PACK(size, 0));
        bp = prev_bp;

    } else if (prev_alloc && !next_alloc) { // 3. next만 free: 2-way 병합
        remove_from_free_list(bp);
        remove_from_free_list(next_bp);

        size += GET_SIZE(HEADER_PTR(next_bp));
        PUT(HEADER_PTR(bp), PACK(size, 0));
        PUT(FOOTER_PTR(next_bp), PACK(size, 0));
        // bp = prev_bp;

    } else {    // 4. prev, next 모두 할당됨: 병합 없음
        return bp;
    }

    insert_into_free_list(bp);
    return bp;
}


void *find_fit_block(size_t size) {
    void* fitBlock;
    int listIndex;
    size_t currentSize = MIN_BLOCK_SIZE;    // 최소 사이즈는 MIN_BLOCK_SIZE

    /* 작은 크기의 list부터 찾아가며 fitBlock을 찾아나간다. 이 때, 오름차순으로 insert했으므로 가장 첫 번째로 찾은 block이 가장 optimal하다. */
    for (listIndex = 0; listIndex < MAX_FREELISTS; listIndex++, currentSize *= 2) {
        if (size <= currentSize && segregatedFreeLists[listIndex] != NULL) {
            fitBlock = segregatedFreeLists[listIndex];
            while (fitBlock != NULL && size > GET_SIZE(HEADER_PTR(fitBlock))) {
                fitBlock = NEXT_NODE(fitBlock);
            }
            if (fitBlock != NULL) return fitBlock;
        }
    }
    return NULL;
}

void *mm_realloc(void *bp, size_t size) {
    if (bp == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    size_t oldBlockSize = GET_SIZE(HEADER_PTR(bp));
    size_t newAlignedSize = ALIGN(size + OVERHEAD);
    if (newAlignedSize < MIN_BLOCK_SIZE) newAlignedSize = MIN_BLOCK_SIZE;

    /* In-place strategy */
    /* 만약 새로 요청한 size가 기존의 size 이하라면 기존에 사용하던 block 그대로 사용 */
    if (newAlignedSize <= oldBlockSize) {
        return allocate_block(bp, newAlignedSize);
    }
    
    void* next_bp = NEXT_BLOCK_PTR(bp);
    size_t nextSize = GET_SIZE(HEADER_PTR(next_bp));

    /* Expand: 새로 memory 할당할 때, 뒤의 block이 free block이고,
       해당 block의 사이즈와 기존에 할당받은 사이즈의 합이 요청한 사이즈보다 크다면 두 block을 합친 block을 return해 malloc, memcpy 호출을 최소화
       이 때, 요청한 공간보다 합친 block의 크기가 크다면 남는 공간은 segregated list에 돌려준다. */
    if (!GET_ALLOC(HEADER_PTR(next_bp))) {
        if (oldBlockSize + nextSize >= newAlignedSize) {
            remove_from_free_list(next_bp);
            /* Expand block: ptr + next_bp */
            size_t totalSize = oldBlockSize + nextSize;
            PUT(HEADER_PTR(bp), PACK(newAlignedSize, 1));
            PUT(FOOTER_PTR(bp), PACK(newAlignedSize, 1));

            /* 남는 것은 segregated list에 돌려준다. */
            size_t remain = totalSize - newAlignedSize;
            void* remainBlock = NEXT_BLOCK_PTR(bp);
            PUT(HEADER_PTR(remainBlock), PACK(remain, 0));
            PUT(FOOTER_PTR(remainBlock), PACK(remain, 0));
            insert_into_free_list(remainBlock);
            return bp;
        }
    }
    
    /* 위의 경우에 해당되지 않는다면 malloc 실행 */
    void *new_bp = mm_malloc(size);
    if (!new_bp) return NULL;

    size_t oldPayload = (oldBlockSize > OVERHEAD) ? (oldBlockSize - OVERHEAD) : 0;
    size_t copySize = (size < oldPayload) ? size : oldPayload;
    memcpy(new_bp, bp, copySize);
    mm_free(bp);

    return new_bp;
}

static void *allocate_block(void *bp, size_t alignedSize) {
    size_t allocatedSize = GET_SIZE(HEADER_PTR(bp));
    remove_from_free_list(bp);
    
    if (allocatedSize - alignedSize <= 16) {    /* 1. 남는 크기가 작아 그냥 전부 할당 */
        PUT(HEADER_PTR(bp), PACK(allocatedSize, 1));
        PUT(FOOTER_PTR(bp), PACK(allocatedSize, 1));

    } else if (allocatedSize - alignedSize > 16 && allocatedSize - alignedSize <= 240) {    /* 2. 남는 크기가 어느 정도(17-240) 있으므로 남는 크기만큼 앞부분은 free list에 넣는다. */
        PUT(HEADER_PTR(bp), PACK(allocatedSize - alignedSize, 0));
        PUT(FOOTER_PTR(bp), PACK(allocatedSize - alignedSize, 0));
        PUT(HEADER_PTR(NEXT_BLOCK_PTR(bp)), PACK(alignedSize, 1));
        PUT(FOOTER_PTR(NEXT_BLOCK_PTR(bp)), PACK(alignedSize, 1));
        insert_into_free_list(bp);
        return NEXT_BLOCK_PTR(bp);

    } else {    /* 3. 남는 크기가 큰 경우에는 앞쪽을 할당하고 나머지 뒷부분은 free list에 넣는다. */
        PUT(HEADER_PTR(bp), PACK(alignedSize, 1));
        PUT(FOOTER_PTR(bp), PACK(alignedSize, 1));
        PUT(HEADER_PTR(NEXT_BLOCK_PTR(bp)), PACK(allocatedSize - alignedSize, 0));
        PUT(FOOTER_PTR(NEXT_BLOCK_PTR(bp)), PACK(allocatedSize - alignedSize, 0));
        insert_into_free_list(NEXT_BLOCK_PTR(bp));
    }

    return bp;
}
