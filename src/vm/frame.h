#ifndef VM_SWAP_H__
#define VM_SWAP_H__

#include <list.h>
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"

struct frame
  {
    void *kpage;
    void *upage;

    struct thread *t;

    struct list_elem list_elem;
  };

void frame_init (void);
void *falloc_get_page(enum palloc_flags, void *);
void  falloc_free_page (void *);
void evict_page();
struct frame *get_frame (void* );

#endif