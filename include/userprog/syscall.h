#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

int exec(char *cmd_line /*, struct intr_frame *_if*/);
int wait(tid_t pid);
void exit (int status);
tid_t fork (const char *thread_name, struct intr_frame *tf);
void halt(void);
void syscall_init (void);
int add_new_file_to_fd_list(struct file *fl, char *file_name);
_Bool create(const char *file, unsigned initial_size);
_Bool remove(const char *file);
int open(char *file_name);
int filesize (int fd);
int read(int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
struct file* get_file_from_fd(int fd);
struct file* get_file_from_name(char *file_name);
void remove_file_from_fd_list(char *name);

struct lock fsl;


#endif /* userprog/syscall.h */
