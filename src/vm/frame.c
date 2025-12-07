#include "vm/frame.h"
#include "threads/synch.h"
#include "vm/swap.h"

static struct list frame_table;
static struct lock frame_lock;
static struct frame *clock_cursor;

void
frame_init ()
{
  list_init (&frame_table);
  lock_init (&frame_lock);
  clock_cursor = NULL;
}

void *
falloc_get_page(enum palloc_flags flags, void *upage)
{
  struct frame *e;
  void *kpage;
  lock_acquire (&frame_lock);
  kpage = palloc_get_page (flags);
  if (kpage == NULL)
  {
    evict_page();
    kpage = palloc_get_page (flags);
    if (kpage == NULL)
      return NULL;
  }
  
  e = (struct frame *)malloc (sizeof *e); //// 실제 frame 들은 malloc으로 메모리 할당. 애초에 해당 메모리는 evict 되면 안되기 때문에, 따로 관리되어야 하는게 맞음.
  e->kpage = kpage;
  e->upage = upage;
  e->t = thread_current ();
  list_push_back (&frame_table, &e->list_elem);

  lock_release (&frame_lock);
  return kpage;
}


void
falloc_free_page (void *kpage)
{
  struct frame *e;
  lock_acquire (&frame_lock);
  e = get_frame (kpage);
  if (e == NULL)
    sys_exit (-1);

  list_remove (&e->list_elem);
  palloc_free_page (e->kpage);
  pagedir_clear_page (e->t->pagedir, e->upage);
  free (e); /// frame 메모리 해제

  lock_release (&frame_lock);
}


struct frame *
get_frame (void* kpage)
{
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
    if (list_entry (e, struct frame, list_elem)->kpage == kpage)
      return list_entry (e, struct frame, list_elem);
  return NULL;
}

// 
void evict_page() {
  ASSERT(lock_held_by_current_thread(&frame_lock));

  struct frame *e = clock_cursor;
  struct spte *s;

  while (1) {
    e = list_pop_front(&frame_table);
    if (!pagedir_is_accessed(e->t->pagedir, e->upage) || !pagedir_is_accessed(e->t->pagedir, e->kpage)) {
        //list_push_back(frame_table, e);
        break;
    }
    else {
        pagedir_set_accessed(e->t->pagedir, e->upage, false);
        pagedir_set_accessed(e->t->pagedir, e->kpage, false);
        list_push_back(&frame_table, e);
    }
  }


  s = get_spte(&thread_current()->spt, e->upage);
  s->status = PAGE_SWAP;
  swap_out(s, e->kpage);

  palloc_free_page (e->kpage);
  pagedir_clear_page (e->t->pagedir, e->upage);
  free (e); /// frame 메모리 해제
  
//   lock_release(&frame_lock); {
//     falloc_free_page(e->kpage);
//   } lock_acquire(&frame_lock);
}
