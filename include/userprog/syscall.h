#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

void syscall_init (void);

struct lock prcs_lock;

_Bool create(const char *file, unsigned initial_size);
_Bool remove(const char *file);
int open(const char *file);
int filesize (int fd);
int read(int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek(int fd, size_t position);
unsigned tell(int fd);
void close(int fd);


#endif /* userprog/syscall.h */
