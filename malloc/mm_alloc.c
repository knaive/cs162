/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>

struct block {
    int free;
    size_t size;
    struct block* prev;
    struct block* next;
};
static struct block _head;

/* if ptr isn't a start of a previously allocated memory, abort it */
static void validate(void* ptr) {
    struct block* p = _head.next;
    while(p) {
        if((void*)p+sizeof(*p)==ptr) return;
        p = p->next;
    }
    fputs("Invalid pointer passed to free\n", stderr);
    abort();
}

static void insert(struct block* item) {
    static struct block* _tail = NULL;
    if(!_tail) _tail = &_head;
    _tail->next = item;
    item->prev = _tail;
    item->next = NULL;
    _tail = item;
}

static struct block* find_large_free(size_t size) {
    struct block* p = &_head;
    while(p!=NULL) {
        if (p->free && p->size>size+sizeof(struct block)) return p;
        p = p->next;
    }
    return NULL;
}

static void* try_split(size_t size) {
    struct block* p = find_large_free(size);
    if(!p) return NULL;

    if(p->size < size+2*sizeof(struct block)) {
        p->size = size;
        p->free = 0;
    }else {
        struct block* next = (struct block*)((void*)p+size+sizeof(*p));
        next->next = p->next;
        next->prev = p;
        next->free = 1;
        next->size = p->size-size-sizeof(*next);

        p->size = size;
        p->next = next;
        p->free = 0;
    }
    return (void*)p+sizeof(struct block);
}

void *mm_malloc(size_t size) {
    if (!size) return NULL;

    void* p = try_split(size);
    if (p) return p;

    struct block* tail = (struct block*)sbrk(0);
    if (sbrk(size+sizeof(struct block))!=tail) return NULL;
    tail->free = 0;
    tail->size = size;
    insert(tail);
    void* ret = (void*)tail+sizeof(*tail);
    bzero(ret, size);
    return ret;
}

void *mm_realloc(void *ptr, size_t size) {
    if (!size) {
        if (ptr) mm_free(ptr);
        return NULL;
    } else if (!ptr) {
        return mm_malloc(size);
    }

    /* if ptr isn't a start of a previously allocated memory, just abort it */
    validate(ptr);

    struct block* cur = (struct block*)((char*)ptr-sizeof(struct block));
    if ((void*)cur->next-(void*)cur-sizeof(struct block)>size) {
        cur->size = size;
        return ptr;
    }

    struct block* new = mm_malloc(size);
    if (!new) return NULL;

    memcpy((void*)new+sizeof(struct block), ptr, cur->size);
    free(ptr);

    return new;
}

void mm_free(void *ptr) {
    if (!ptr) return;

    validate(ptr);

    struct block* current = (struct block*)(ptr-sizeof(struct block));
    struct block* next = current->next;
    current->free = 1;
    while(next&&next->free) {
        if (next->next) {
            current->size = (void*)(next->next)-(void*)current-sizeof(struct block);
            current->next = next->next;
            next->prev = current;
        } else {
            void* end = sbrk(0);
            current->size = end-(void*)current-sizeof(struct block);
            current->next = NULL;
        }
        next = current->next;
    }

    /* merge the previous free memory block with current, 
     * there is only a free block in the previous ones 
     * */
    struct block* prev = current->prev;
    if(prev->free) {
        prev->next = next;
        if(next) next->prev = prev;
        prev->size += (void*)current-(void*)prev + current->size;
    }
}
