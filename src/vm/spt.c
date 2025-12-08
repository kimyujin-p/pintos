#include "vm/spt.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include <string.h>
#include "threads/vaddr.h"
#include "vm/swap.h"

static hash_hash_func spt_hash_func;
static hash_less_func spt_less_func;
static void spte_destrutcor (struct hash_elem *elem, void *aux);
extern struct lock file_lock;

void
init_spt (struct hash *spt)
{
  hash_init (spt, spt_hash_func, spt_less_func, NULL);
}

void
destroy_spt (struct hash *spt)
{
  hash_destroy (spt, spte_destrutcor);
}


void
init_zero_spte (struct hash *spt, void *upage)
{
  struct spte *e;
  e = (struct spte *) malloc (sizeof *e);
  
  e->upage = upage;
  e->kpage = NULL;
  
  e->status = PAGE_ZERO;
  
  e->file = NULL;
  e->writable = true;
  
  hash_insert (spt, &e->hash_elem);
}

void
init_frame_spte (struct hash *spt, void *upage, void *kpage)
{
  struct spte *e;
  e = (struct spte *) malloc (sizeof *e);

  e->upage = upage;
  e->kpage = kpage;
  
  e->status = PAGE_FRAME;

  e->file = NULL;
  e->writable = true;
  
  hash_insert (spt, &e->hash_elem);
}

struct spte *
init_file_spte (struct hash *spt, void *_upage, struct file *_file, off_t _ofs, uint32_t _read_bytes, uint32_t _zero_bytes, bool _writable)
{
  struct spte *e;
  
  e = (struct spte *)malloc (sizeof *e);

  e->upage = _upage;
  e->kpage = NULL;
  
  e->file = _file;
  e->ofs = _ofs;
  e->read_bytes = _read_bytes;
  e->zero_bytes = _zero_bytes;
  e->writable = _writable;
  
  e->status = PAGE_FILE;
  
  hash_insert (spt, &e->hash_elem);
  
  return e;
}

bool
load_page (struct hash *spt, void *upage) // page fault handler 에서 사용
{
  struct spte *e;
  uint32_t *pagedir;
  void *kpage;

  e = get_spte (spt, upage);
  if (e == NULL)
    sys_exit (-1);

  kpage = falloc_get_page (PAL_USER, upage);
  if (kpage == NULL) 
    sys_exit (-1);

  bool already_holding_lock = (&file_lock != NULL) && lock_held_by_current_thread(&file_lock);

  switch (e->status)
  {
  case PAGE_ZERO:
    memset (kpage, 0, PGSIZE);
    break;
  case PAGE_SWAP:
    swap_in(e, kpage);
  
    break;
  case PAGE_FILE:
    if (!already_holding_lock)
      lock_acquire (&file_lock);
    
    if (file_read_at (e->file, kpage, e->read_bytes, e->ofs) != e->read_bytes)
    {
      falloc_free_page (kpage);
      if (!already_holding_lock) // 좀 이상한듯 
        lock_release (&file_lock);
      sys_exit (-1);
    }
    
    memset (kpage + e->read_bytes, 0, e->zero_bytes);
    if (!already_holding_lock)
      lock_release (&file_lock);

    break;

  default:
    sys_exit (-1);
  }
    
  pagedir = thread_current ()->pagedir;

  if (!pagedir_set_page (pagedir, upage, kpage, e->writable))
  {
    falloc_free_page (kpage);
    sys_exit (-1);
  }

  e->kpage = kpage;
  e->status = PAGE_FRAME;

  return true;
}

struct spte *
get_spte (struct hash *spt, void *upage)
{
  struct spte e;
  struct hash_elem *elem;

  e.upage = upage;
  elem = hash_find (spt, &e.hash_elem);

  return elem != NULL ? hash_entry (elem, struct spte, hash_elem) : NULL;
}

static unsigned
spt_hash_func (const struct hash_elem *elem, void *aux)
{
  struct spte *p = hash_entry(elem, struct spte, hash_elem);

  return hash_bytes (&p->upage, sizeof (p->upage));
}

static bool 
spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  void *a_upage = hash_entry (a, struct spte, hash_elem)->upage;
  void *b_upage = hash_entry (b, struct spte, hash_elem)->upage;

  return a_upage < b_upage;
}

static void
spte_destrutcor (struct hash_elem *elem, void *aux)
{
  struct spte *e;

  e = hash_entry (elem, struct spte, hash_elem);

  if (e->status == PAGE_FRAME)
  {
    falloc_free_page (e->kpage); // frame 해제
  }

  else if (e->status == PAGE_SWAP)
  {
    delete_in_swap_disk(e->swap_id); // swap에서 페이지 제거
  }

  free(e);
}

void 
spte_delete (struct hash *spt, struct spte *entry)
{
  hash_delete (spt, &entry->hash_elem);
  free (entry);
}