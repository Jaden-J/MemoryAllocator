/* Author:  Steely Morneau
 * Purpose: A thread-safe user-level memory allocator
 * Usage:   Use makefile to build. ./test_mem to run.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "pintos/memalloc.h"
#include <pthread.h>

struct list free_list;
pthread_mutex_t gmutex = PTHREAD_MUTEX_INITIALIZER;

/* Initialize the free list */
void 
mem_init(uint8_t *base, size_t length) 
{
    struct free_block *fb;
    struct list_elem elem;

    fb = (struct free_block *) base;
    fb->length = length;
    fb->elem = elem;

    list_init(&free_list);
    list_push_back(&free_list, &fb->elem);
}

/* Allocate 'length' bytes of memory. Assume that size is a multiple of 4. */
void * 
mem_alloc(size_t length) 
{
    struct list_elem *e;
    void *end_of_fb = NULL, *ub = NULL;
    long leftover = 0, size_needed = 0;
    long min_alloc_size = sizeof(struct free_block) - sizeof(struct used_block);

    pthread_mutex_lock(&gmutex);

    /* if the user tries to allocate a space that will be too small to be freed */
    if(length < min_alloc_size) 
    {
        length = min_alloc_size;
    }

    size_needed = length + sizeof(struct used_block);

    for (e = list_begin(&free_list); e != list_end(&free_list); e = list_next(e)) 
    {
        struct free_block *fb = list_entry(e, struct free_block, elem);

        /* Check if current free block has enough space */
        if(fb->length < size_needed) 
        {
            continue;
        }

        leftover = fb->length - size_needed;

        /* if space after allocation is less than free block header 
            give it to the used block and remove the free block     */
        if(leftover < (long)(sizeof(struct free_block))) 
        {
            length += leftover;
            size_needed = length + sizeof(struct used_block);
            list_remove(e);
        }

        /* update the free block length and instantiate the used block */
        end_of_fb = (void *)((size_t)fb + (size_t)fb->length);
        ub = (void*)(end_of_fb - size_needed);
        fb->length = (size_t)(fb->length - size_needed);
        ((struct used_block *)ub)->length = length;

        pthread_mutex_unlock(&gmutex);
        return ((struct used_block *)ub)->data;
    }

    pthread_mutex_unlock(&gmutex);

    /* No space for allocation */
    return NULL;
}

/* Compare two list_elems by their memory address */
bool 
compare (const struct list_elem *a, const struct list_elem *b, void *aux) 
{

   if ((long)a < (long)b) 
   {
        return -1;
   }  
   else if ((long)a == (long)b) 
   {
      return 0;
   }
   else 
   {
      return 1;
   }
}

/* Free memory pointed to by 'ptr'. */
void 
mem_free(void *ptr) 
{
    struct list_elem *e, *e_to_remove;
    void * ub = ( ptr - sizeof(struct used_block) );
    void *fb_before = NULL, *fb_after = NULL;
    long available_space = 0;
    pthread_mutex_lock(&gmutex);

    if(ptr == NULL) 
    {
        return;
    }

    available_space = (long)sizeof(struct used_block) + (long)((struct used_block *)ub)->length;

    /* find free blocks surrounding used block */
    for (e = list_begin(&free_list); e != list_end(&free_list); e = list_next(e)) 
    {
        struct free_block *fb = list_entry(e, struct free_block, elem);

        /* check if free mem space before is adjacent to used block */
        if(ub == (void*)fb + ((struct free_block *)fb)->length ) 
        {
            fb_before = fb;
        }

        /* check if mem space after is adjacent to used block */
        if(fb == (void *)(ptr + ((struct used_block *)ub)->length)) 
        {
            fb_after = fb;
            e_to_remove = e;
        }
    }

    /* insert new free block; don't coalesce */
    if(fb_before == NULL && fb_after == NULL) 
    {
        struct list_elem e;
        struct free_block *fb = (struct free_block *) ub;

        fb->length = available_space;
        fb->elem = e;
        list_insert_ordered(&free_list, &(fb->elem), compare, NULL);
    }

    /* coalesce up but not down */
    else if(fb_before != NULL && fb_after == NULL) 
    {
        ((struct free_block *)fb_before)->length += available_space;
    }

    /* coalesce down but not up */
    else if(fb_before == NULL && fb_after != NULL) 
    {
        struct list_elem e;
        struct free_block *fb = (struct free_block *) ub;

        /* create new free_block with used block address and updated length */
        fb->length = available_space + ((struct free_block *)fb_after)->length;
        fb->elem = e;
        list_insert_ordered(&free_list, &(fb->elem), compare, NULL);
        list_remove(e_to_remove);
    }

    /* coalesce both ways */
    else 
    {
        /* update free_block length to include used block and fb_after; remove fb_after */
        ((struct free_block *)fb_before)->length += available_space;
        ((struct free_block *)fb_before)->length += ((struct free_block *)fb_after)->length;
        list_remove(e_to_remove);
    }

    pthread_mutex_unlock(&gmutex);
}

/* Return the number of elements in the free list. */
size_t 
mem_sizeof_free_list(void) 
{
    return list_size(&free_list);
}

/* Dump the free list. */
void 
mem_dump_free_list(void) 
{
    struct list_elem *e;

    for (e = list_begin(&free_list); e != list_end(&free_list); e = list_next(e)) {
        struct free_block *fb = list_entry(e, struct free_block, elem);
        printf("\t%p %d\n", fb, (int)fb->length);
    }
}