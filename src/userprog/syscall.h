#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void sys_exit (int status);
void sys_close (int fd);

#endif /* userprog/syscall.h */
