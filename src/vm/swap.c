#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct bitmap *swap_bitmap;
static struct block *swap_block;
static struct lock swap_lock;

#define SLOT_SIZE (PGSIZE / BLOCK_SECTOR_SIZE)

void
swap_init (void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  if (swap_block == NULL)
    PANIC ("No swap block device found");

  size_t swap_block_size = block_size (swap_block);
  size_t num_swap_slots = swap_block_size / SLOT_SIZE;

  swap_bitmap = bitmap_create (num_swap_slots);
  if (swap_bitmap == NULL)
    PANIC ("Failed to create swap bitmap");

  bitmap_set_all (swap_bitmap, false);

  lock_init (&swap_lock);

}

void swap_in (struct spte *spte, void *kpage)
{
  int slot_index = spte->swap_id;

  if (slot_index < 0 || slot_index >= bitmap_size (swap_bitmap)) sys_exit(-1);

  lock_acquire (&swap_lock);

  if (!bitmap_test (swap_bitmap, slot_index)) {
    lock_release (&swap_lock);
    sys_exit(-1);
  }

  for (int i = 0; i < SLOT_SIZE; i++){
      block_read (swap_block, slot_index * SLOT_SIZE + i, kpage + i * BLOCK_SECTOR_SIZE);
  }

  bitmap_set (swap_bitmap, slot_index, false);

  lock_release (&swap_lock);
}

void swap_out (struct spte *spte, void *kpage)
{
  lock_acquire (&swap_lock);

  int slot_index = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
  if (slot_index == BITMAP_ERROR) {
    lock_release (&swap_lock);
    sys_exit(-1);
  }

  for (int i = 0; i < SLOT_SIZE; i++){
      block_write (swap_block, slot_index * SLOT_SIZE + i, kpage + i * BLOCK_SECTOR_SIZE);
  }

  spte->swap_id = slot_index;

  lock_release (&swap_lock);
}

void delete_in_swap_disk (int slot_index)
{
  lock_acquire (&swap_lock);

  if (slot_index < 0 || slot_index >= bitmap_size (swap_bitmap)) {
    lock_release (&swap_lock);
    sys_exit(-1);
  }

  if(!bitmap_test (swap_bitmap, slot_index)) {
    lock_release (&swap_lock);
    sys_exit(-1);
  }

  bitmap_set (swap_bitmap, slot_index, false);

  lock_release (&swap_lock);
}