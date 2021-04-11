#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct pointer_elem{
		uintptr_t pointer_address;
		struct list_elem elem;
	};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
bool load (const char *file_name, struct intr_frame *if_);

#endif /* userprog/process.h */
