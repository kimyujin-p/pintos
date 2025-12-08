#ifndef VM_MMAP_H__
#define VM_MMAP_H__

#include "filesys/file.h"

typedef struct mmap_entry {
    int mapping_id;
    struct file *file;
    void *upage;
    struct list_elem elem;
} mmap_entry;

void init_mmap_list (void);
int make_mmap_entry (struct file *file, void *addr, int file_length);
void remove_mmap_entry (int mapping_id);
mmap_entry* get_mmap_entry (int mapping_id);

#endif /* vm/mmap.h */