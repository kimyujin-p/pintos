#include "mmap.h"
#include "threads/thread.h"
#include "vm/spt.h"
#include "filesys/file.h"
extern struct lock file_lock;

void init_mmap_list (void)
{
    struct thread *cur = thread_current();
    list_init (&cur->mmap_list);
    cur->next_mapping_id = 1;
}

int make_mmap_entry (struct file *file, void *addr, int file_length)
{
    struct thread *cur = thread_current();
    int next_mapping_id = cur->next_mapping_id++; 
    struct hash *spt = &cur->spt;

    mmap_entry *entry = (mmap_entry *)malloc (sizeof (mmap_entry));
    if (entry == NULL)
        return -1;

    for (int offset = 0; offset < file_length; offset += PGSIZE)
    {
        void *upage = addr + offset;
        if (get_spte (spt, upage) != NULL)
        {
            free(entry);
            return -1; 
        }
    }

    for (int offset = 0; offset < file_length; offset += PGSIZE)
    {
        void *upage = addr + offset;
        uint32_t read_bytes = file_length - offset < PGSIZE ? file_length - offset : PGSIZE;
        uint32_t zero_bytes = PGSIZE - read_bytes;

        init_file_spte (spt, upage, file, offset, read_bytes, zero_bytes, true);
    }

    entry->mapping_id = next_mapping_id++;
    entry->file = file;
    entry->upage = addr;

    list_push_back (&cur->mmap_list, &entry->elem);

    return entry->mapping_id;
}

mmap_entry* get_mmap_entry (int mapping_id)
{
    struct thread *cur = thread_current();
    struct list *mmap_list = &cur->mmap_list;
    struct list_elem *e;

    for (e = list_begin (mmap_list); e != list_end (mmap_list); e = list_next (e))
    {
        mmap_entry *entry = list_entry (e, mmap_entry, elem);
        if (entry->mapping_id == mapping_id)
        {
            return entry;
        }
    }
    return NULL; 
}

