#ifndef VM_SWAP_H__
#define VM_SWAP_H__

#include "vm/spt.h"

void swap_init (void);
void swap_in (struct spte *spte, void *kpage);
void swap_out (struct spte *spte, void *kpage);
void delete_in_swap_disk (int slot_index);

#endif