#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "threads/synch.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "string.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "userprog/process.h"

void is_safe_access(void *ptr);
static struct list fd_list;
static int fd_counter;

struct fd_file {
	int fd;
	struct file *file;
	char *file_name;
	struct list_elem elem;
};



void is_safe_access(void *ptr) {
	if (ptr == NULL || is_kernel_vaddr(ptr) ) {
		exit(-1);
	}
}

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);
	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	// Initialize the fd list
	list_init(&fd_list);
	fd_counter = 2;

	lock_init(&fsl);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	int rax = f->R.rax;
	// printf("Shehehe\n");
	switch (rax)
	{
	/* Projects 2 and later. */
	case SYS_HALT:                  /* Halt the operating system. */
	{
		halt();
		break;
	}
	case SYS_EXIT:                   /* Terminate this process. */
	{
		int status = f->R.rdi;
		exit(status);
		break;
	}
	case SYS_FORK:                   /* Clone current process. */ 
	{
		const char *thread_name = (char *) f->R.rdi;
		tid_t response = fork(thread_name, f);
		f->R.rax = response;
		break;
	}
	case SYS_EXEC:                   /* Switch current process. */
	{
		char *cmd_line = (char *) f->R.rdi;
		f->R.rax = exec(cmd_line);
		break;
	}
	case SYS_WAIT:                   /* Wait for a child process to die. */
	{
		tid_t pid = (int) f->R.rdi;

		int exit_status = wait(pid);
		f->R.rax = exit_status;
		break;
	}
	case SYS_CREATE:                 /* Create a file. */
	{
		char *file_name = (char *) f->R.rdi;
		int initial_size =  (int) f->R.rsi;
		bool response = create(file_name, initial_size);	
		f->R.rax = response;
		break;
	}
	case SYS_REMOVE:                 /* Delete a file. */ 
	{
		char *file_name = (char *)f->R.rdi;
		int response = remove(file_name);		
		f->R.rax = response;
		break;
	}
	case SYS_OPEN:                   /* Open a file. */
	{
		char *file_name = (char *) f->R.rdi;
		int response = open(file_name);	
		f->R.rax = response;
		break;
	}
	case SYS_FILESIZE:               /* Obtain a file's size. */ 
	{
		int fd = f->R.rdi;
		int response = filesize(fd);
		f->R.rax = response;
		break;
	}
	case SYS_READ:                   /* Read from a file. */ 
	{
		int fd = f->R.rdi;
		char *buf = (char *) f->R.rsi;
		unsigned str_len = (unsigned) f->R.rdx;
		int response = read(fd, buf, str_len);
		f->R.rax = response;
		break;
	}
	case SYS_WRITE:                  /* Write to a file. */  
	{
		char *buf = (char *) f->R.rsi;
		int fd = f->R.rdi;
		unsigned str_len = f->R.rdx;
		int response = write(fd, buf, str_len);
		f->R.rax = response;
		break;
	}
	case SYS_SEEK:                   /* Change position in a file. */  {
		int fd = f->R.rdi;
		char position = f->R.rsi;
		seek(fd, position);	
		break;
	}
	case SYS_TELL:                   /* Report current position in a file. */
	{
		int fd = f->R.rdi;
		int response = tell(fd);
		f->R.rax = response;
		break;	
	}
	case SYS_CLOSE:                  /* Close a file. */
	{
		int fd = f->R.rdi;
		close(fd);
		break;
	}
	
	/* Extra for Project 2 */ 
	case SYS_DUP2:                   /* Duplicate the file descriptor */

	case SYS_MOUNT:
	case SYS_UMOUNT:
	default:
		break;
	}

}

void halt(void) {
	power_off();
}

void exit (int status) {
	thread_current()->status = status;
	thread_exit ();
}

tid_t fork (const char *thread_name, struct intr_frame *tf) {
	is_safe_access((void *)thread_name);

	/* 1. Clone the current process and change name to thread_name */
	// Registers to be cloned: %rbx, %rsp, %rbp and %r12-%r15
	return process_fork(thread_name, tf);
}

int wait(tid_t pid) {
	// Wait for the child process
	return process_wait(pid);

}

int exec(char *cmd_line /*, struct intr_frame *_if*/) {
	is_safe_access((void *) cmd_line);
	char *cpy_cmd_line = malloc(strlen(cmd_line) + 1);
	strlcpy(cpy_cmd_line, cmd_line, strlen(cmd_line) + 1);
	return process_create_initd(cpy_cmd_line);
}


/* File functions*/

bool create(const char *file_name, unsigned initial_size){
	printf("Inside a create\n");
	is_safe_access((void *) file_name);
	printf("After access a create\n");
	if (file_name == NULL || strlen(file_name) == 0) {
		exit(-1);
		return false;
	}
	if (strlen(file_name) > 256) {
		return false;
	}
	// lock_acquire(&fsl);
	struct file *file = filesys_open(file_name);
	// lock_release(&fsl);
	if (file != NULL) {
		file_close(file);
		return false;
	} 
	// lock_acquire(&fsl);
	bool is_created = filesys_create(file_name, initial_size);
	// lock_release(&fsl);
	return is_created;
};

bool remove(const char *file){
	is_safe_access((void *) file);
	lock_acquire(&fsl);
	bool is_closed = filesys_remove(file);
	lock_release(&fsl);
	return is_closed;
}

int open(char *file_name){
	is_safe_access((void *)file_name);
	lock_acquire(&fsl);
	struct file *file = filesys_open(file_name);
	lock_release(&fsl);
	if (file == NULL) {
		return -1;
	} else {
		int fd = add_new_file_to_fd_list(file, file_name);
		return fd;
	}
};

int filesize (int fd){ 
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		return 0;
	}	
	return file_length(file);
}

int read(int fd, void *buffer, unsigned size){
	is_safe_access(buffer);
	int response;
	if (fd == 0) {
		response = input_getc();
	} else {
		struct file *fl = get_file_from_fd(fd);
		if (fl == NULL) {
			return -1;
		}
		response = file_read(fl, buffer, size);
	}
	return response;
}

int write (int fd, void *buffer, unsigned size){
	is_safe_access(buffer);
	if (fd == 1) {
	putbuf(buffer, size);
	return size;
	} else {
		struct file *file = get_file_from_fd(fd);
		if (file == NULL) {
			return 0;
		}
		int read_size = file_write(file, buffer, size);
		return read_size;
	}
}

void seek(int fd, unsigned position){
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		return;
	}
	file_seek(file, position);
}

unsigned tell(int fd) {
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		return 0;
	}
	int response = file_tell(file);
	if (response < 0 ) {
		response = 0;
	}
	return (unsigned) response;
};

void close(int fd){
	for(struct list_elem *e = list_begin(&fd_list); e != list_end(&fd_list); e = list_next(e)) {
		struct fd_file *a = list_entry(e, struct fd_file, elem);
		if (a->fd == fd) {
			file_close(a->file);
			list_remove(e);
		}
		if (a->fd > fd) {
			break;
		}
	}
};


// File Fd functions
int add_new_file_to_fd_list(struct file *fl, char *file_name) {
	// Create a fd struct
	struct fd_file *new_fd_file = malloc(sizeof(struct fd_file));
	new_fd_file->fd = fd_counter;
	new_fd_file->file = fl;
	new_fd_file->file_name = file_name;
	fd_counter++;
	list_push_back(&fd_list, &new_fd_file->elem);
	return new_fd_file->fd;
}

struct file* get_file_from_fd(int fd) {
	for(struct list_elem *e = list_begin(&fd_list); e != list_end(&fd_list); e = list_next(e)) {
		struct fd_file *a = list_entry(e, struct fd_file, elem);
		if (a->fd == fd) {
			return a->file;
		}
		if (a->fd > fd) {
			return NULL;
		}
	}
	return NULL;
}


struct file* get_file_from_name(char *file_name) {
	for(struct list_elem *e = list_begin(&fd_list); e != list_end(&fd_list); e = list_next(e)) {
		struct fd_file *a = list_entry(e, struct fd_file, elem);
		if (strcmp(a->file_name, file_name) == 0) {
			return a->file;
		}
	}
	return NULL;
}

void remove_file_from_fd_list(char *name) {
	for(struct list_elem *e = list_begin(&fd_list); e != list_end(&fd_list); e = list_next(e)) {
		struct fd_file *a = list_entry(e, struct fd_file, elem);
		if (strcmp(a->file_name, name) == 0) {
			list_remove(e);
			return;
		}
	}
}