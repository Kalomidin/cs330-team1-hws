#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"

struct pointer_elem{
		uintptr_t pointer_address;
		struct list_elem elem;
};

struct file_information {
	int fd;
	struct file *file;
	struct list_elem elem;
};

struct child_info {
	struct semaphore *sema;
	tid_t pid;
	char *file_name;
	int *exit_status;
	struct list_elem elem;
};

struct lazy_aux{
	struct file *file;
	off_t ofs;
	size_t page_read_bytes;
	size_t page_zero_bytes;
};

struct semaphore fork_sema; // Use for synchronizing fork


tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#endif /* userprog/process.h */
