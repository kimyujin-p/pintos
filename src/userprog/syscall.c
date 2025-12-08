  #include "userprog/syscall.h"
  #include <stdio.h>
  #include <syscall-nr.h>
  #include "threads/interrupt.h"
  #include "threads/thread.h"
  //for assn2
  #include "threads/vaddr.h"       
  #include "userprog/pagedir.h"
  #include "devices/shutdown.h"
  #include "userprog/process.h"
  #include "filesys/filesys.h"
  #include "filesys/file.h"
  #include "threads/synch.h"
  #include "devices/input.h"
  #include "console.h"
  //for assn2 end

  struct lock file_lock;

  static void syscall_handler (struct intr_frame *);
  struct thread* get_child(tid_t pid);
  void valid_addr(const void *vaddr);
  void get_argument(void *esp, int *argv, int argc);
  void sys_halt(void);
  void sys_exit(int status);
  tid_t sys_exec(const char *cmd_line);
  int sys_wait(tid_t pid);
  
  bool sys_create (const char *file, unsigned initial_size);
  bool sys_remove (const char *file);
  int  sys_open   (const char *file);
  int  sys_filesize (int fd);
  int  sys_read   (int fd, void *buffer, unsigned size);
  int  sys_write  (int fd, const void *buffer, unsigned size);
  void sys_seek   (int fd, unsigned position);
  unsigned sys_tell (int fd);
  void sys_close  (int fd);

  void
  syscall_init (void) 
  {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init (&file_lock);
  }

  static void
  syscall_handler (struct intr_frame *f) 
  {
    valid_addr(f->esp);  //validate addr test
    int syscall_num = *(int *)(f->esp);   //어떤 system call 인지 파악
    int argv[3];

    // for project3 -> page fault handler에서 interrupt 발생 시점의 thread의 esp 획득에 필요.
    // 만약 유저 모드였다면 f->esp 그대로 사용. 커널 모드였다면 thread_current()->esp를 사용해야함. 
    thread_current()->esp = f->esp;

    switch (syscall_num) {
      case SYS_HALT:
        {
          sys_halt ();
          break;
        }
      case SYS_EXIT:
        {
          // 인자로 전달된 status 값 읽기
          //int status = *((int *)f->esp + 1);
          //sys_exit(status);
          get_argument (f->esp + 4, argv, 1);
          sys_exit ((int)argv[0]);
          break;
        }
      case SYS_EXEC:
      {
        get_argument (f->esp + 4, argv, 1);
        valid_addr((const void*)argv[0]);
        f->eax = sys_exec ((const char *)argv[0]);
        break;
      }
      case SYS_WAIT:
      {
         get_argument (f->esp + 4, argv, 1);
        f->eax = sys_wait ((tid_t)argv[0]);
        break;
      }
      case SYS_CREATE:
      {
        get_argument (f->esp + 4, argv, 2);
        f->eax = sys_create ((const char *)argv[0], (unsigned)argv[1]);
        break;
      }
      case SYS_REMOVE:
      {
        get_argument (f->esp + 4, argv, 1);
        f->eax = sys_remove ((const char *)argv[0]);
        break;
      }
      case SYS_OPEN:
      {
        get_argument (f->esp + 4, argv, 1);
        f->eax = sys_open ((const char *)argv[0]);
        break;
      }
      case SYS_FILESIZE:
      {
        get_argument (f->esp + 4, argv, 1);
        f->eax = sys_filesize ((int)argv[0]);
        break;
      }
      case SYS_READ:
      {
        get_argument (f->esp + 4, argv, 3);
        f->eax = sys_read ((int)argv[0], (void *)argv[1], (unsigned)argv[2]);
        break;
      }
      case SYS_WRITE:
      {
        get_argument (f->esp + 4, argv, 3);
        f->eax = sys_write ((int)argv[0], (const void *)argv[1], (unsigned)argv[2]);
        break;
      }
      case SYS_SEEK:
      {
        get_argument (f->esp + 4, argv, 2);
        sys_seek ((int)argv[0], (unsigned)argv[1]);
        break;
      }
      case SYS_TELL:
      {
        get_argument (f->esp + 4, argv, 1);
        f->eax = sys_tell ((int)argv[0]);
        break;
      }
      case SYS_CLOSE:
      {
        get_argument (f->esp + 4, argv, 1);
        sys_close ((int)argv[0]);
        break;
      }
      case SYS_MMAP:
      {
        get_argument (f->esp, argv, 2);
        f->eax = sys_mmap ((int) argv[0], (void *) argv[1]);
        break;
      }
      case SYS_MUNMAP:
      {
        get_argument (f->esp, argv, 1);
        sys_munmap ((int) argv[0]);
        break;
      }
      default:
        printf ("Unknown system call: %d\n", syscall_num);
        sys_exit(-1);
        break;
    }
  }

  void
  valid_addr (const void *addr)
  {
    if ((addr == NULL)
        || (is_user_vaddr (addr) == false)
       || (pagedir_get_page (thread_current ()->pagedir, addr) == NULL))
    {
      sys_exit (-1); //valid하지 않은 경우, process 종료 
    }
  }

  void
  get_argument (void *esp, int *argv, int num)  //각 system call별 필요로 하는 인자만큼 저장시켜주기
  {
    int i;
    uint8_t *u = (uint8_t *)esp;
    for (i = 0; i < num; i++) 
    {
      valid_addr (u + 4 * i);    //validation test 
      argv[i] = *(int *)(u + 4 * i); 
    }
  }

  void
  sys_exit (int status)
  {
    struct thread *cur = thread_current();

    cur->exit_status = status;   // 종료 코드 저장
    printf("%s: exit(%d)\n", cur->name, status);  // 종료 메시지 출력

    thread_exit();  // 스레드 및 프로세스 종료
  }

  void 
  sys_halt (void)
  {
    shutdown_power_off();  //pintos를 종료
  }

  tid_t
  sys_exec(const char *cmd_line)
  {
    struct thread *child;
    tid_t pid;

    valid_addr(cmd_line);
    
    pid = process_execute(cmd_line);
    if(pid == -1)
    {
      return -1;
    }

    child = get_child (pid);
    if(child == NULL) return -1;
    sema_down (&child -> load_sema);     //child의 load완료를 기다린다.

    if(child->is_load)
    {
       return pid;
    }

    else
    {
      return -1;
    }
  }

  struct thread*
  get_child(tid_t pid)
  {
    struct thread *t;
    struct list *child_list = &thread_current() -> children_list;
    struct list_elem *elem;

    //child 목록을 처음부터 끝까지 확인
    for(elem = list_begin (child_list); elem != list_end (child_list); elem = list_next (elem))
    {
      t = list_entry (elem, struct thread, children_elem);
      if (t->tid == pid)  //pid에 해당하는 child를 찾으면 반환 
      {
        return t;
      }
    }
    return NULL;  //못 찾은 경우 NULL반환
  }

  int
  sys_wait (tid_t pid)
  {
    return process_wait(pid);   //child process의 pid를 전달해 해당 process의 종료 waiting
  }

  bool
  sys_create (const char *file, unsigned initial_size)
  {
    valid_addr(file);  //validation test

    if(file ==NULL)
    {
      sys_exit(-1); //file이 NULL이면 에러
    }

    lock_acquire (&file_lock);
    bool ok = filesys_create (file, initial_size);
    lock_release (&file_lock);
    return ok;
  }

  bool
  sys_remove (const char *file)
  {
    valid_addr((void*) file); //validation test

    lock_acquire (&file_lock);
    bool ok = filesys_remove (file);
    lock_release (&file_lock);
    return ok;
  }
  
  int
  sys_open(const char *file)
  {
    struct thread *t = thread_current ();
    struct file *f;
    int fd;

    valid_addr (file);

    lock_acquire (&file_lock);
    f = filesys_open (file);
    if (f == NULL)
    {
      lock_release (&file_lock);
      return -1;
    }
    if (!strcmp (t->name, file))
    file_deny_write (f);

    fd = t->fd_max;
    t->fd_table[fd] = f;
    t->fd_max++;

    lock_release (&file_lock);
    return fd;
  }

  int 
  sys_filesize (int fd)
  {
    struct thread *t = thread_current ();
    struct file *f;
    int len;

    lock_acquire (&file_lock);
    if (fd >= 0 && fd < t->fd_max && t->fd_table[fd] != NULL)
    {
      f = t->fd_table[fd];
      len = file_length (f);
      lock_release (&file_lock);
      return len;
    }
    lock_release (&file_lock);
    return -1;
  }

  int
  sys_read (int fd, void *buffer, unsigned size)
  {
    struct thread *t = thread_current ();
    struct file *f;
    int bytes = 0;
    unsigned i;

    for (i = 0; i < size; i++)
      valid_addr ((uint8_t *) buffer + i);

    if (fd == 0)   // stdin
    {
      for (i = 0; i < size; i++)
        {
          ((char *) buffer)[i] = input_getc ();
          bytes++;
        }
      return bytes;
    }
    else if (fd > 1 && fd < t->fd_max && t->fd_table[fd] != NULL)
    {
      f = t->fd_table[fd];
      lock_acquire (&file_lock);
      bytes = file_read (f, buffer, size);
      lock_release (&file_lock);
      return bytes;
    }
    return -1;
  }

  int
  sys_write (int fd, const void *buffer, unsigned size)
  {
    struct thread *t = thread_current ();
    struct file *f;
    int bytes = 0;
    unsigned i;

  for (i = 0; i < size; i++)
    valid_addr ((const uint8_t *) buffer + i);

  if (fd == 1)   // stdout
    {
      lock_acquire (&file_lock);
      putbuf (buffer, size);
      lock_release (&file_lock);
      return (int) size;
    }
  else if (fd > 1 && fd < t->fd_max && t->fd_table[fd] != NULL)
    {
      f = t->fd_table[fd];
      lock_acquire (&file_lock);
      bytes = file_write (f, buffer, size);
      lock_release (&file_lock);
      return bytes;
    }
    return -1;
  }

  void
  sys_seek(int fd, unsigned position)
  {
    struct thread *t = thread_current ();
    struct file *f;

    lock_acquire (&file_lock);
    if (fd >= 0 && fd < t->fd_max && t->fd_table[fd] != NULL)
    {
      f = t->fd_table[fd];
      file_seek (f, position);
    }
    lock_release (&file_lock);
  }

  unsigned
  sys_tell(int fd)
  {
    struct thread *t = thread_current ();
    struct file *f;
    unsigned pos = 0;

    lock_acquire (&file_lock);
    if (fd >= 0 && fd < t->fd_max && t->fd_table[fd] != NULL)
    {
      f = t->fd_table[fd];
      pos = file_tell (f);
      lock_release (&file_lock);
      return pos;
    }
    lock_release (&file_lock);
    return 0;
  }

  void
  sys_close (int fd)
  {
    struct thread *t = thread_current ();
    struct file *f;

    if (fd <= 1 || fd >= t->fd_max)
      return;

    lock_acquire (&file_lock);
    f = t->fd_table[fd];
    if (f != NULL)
    {
      file_close (f);
      t->fd_table[fd] = NULL;
    }
    lock_release (&file_lock);
  }