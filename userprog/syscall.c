#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "string.h"
#include "threads/mmu.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "string.h"

#define MAX_GENERATION 10

static fd_counter;

static struct file* get_file_from_fd(int fd);
static struct lock fd_counter_lock;


void* mmap (void *addr, size_t length, int writable, int fd, off_t offset){
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		return NULL;
	}
	void * response = do_mmap(addr, length, writable, file, offset);
	return response;
}

void munmap (void *addr){
	return;
}

int add_new_file_to_fd_list(struct file *fl);
struct file* get_file_from_fd(int fd);
int dup2(int oldfd, int newfd);

void is_safe_access(void *ptr) {
	// printf("Ptr is pml4 page: %p, %d\n", ptr, pml4_get_page(thread_current()->pml4, ptr) == NULL);
	if (ptr == NULL || is_kernel_vaddr(ptr) || pml4_get_page(thread_current()->pml4, ptr) == NULL) {
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
	lock_init(&prcs_lock);
	sema_init(&fork_sema, 0);
	lock_init(&fd_counter_lock);
	fd_counter = 2;
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	int rax = f->R.rax;

	// Save rsp to the current thread for page fault handling
	thread_current()->rsp = f->rsp;

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
		char *thread_name = f->R.rdi;
		tid_t response = fork(thread_name, f);
		f->R.rax = response;
		break;
	}
	case SYS_EXEC:                   /* Switch current process. */
	{
		char *cmd_line = f->R.rdi;
		f->R.rax = exec(cmd_line);
		break;
	}
	case SYS_WAIT:                   /* Wait for a child process to die. */
	{
		int pid = f->R.rdi;
		int exit_status = wait(pid);
		f->R.rax = exit_status;
		break;
	}
	case SYS_CREATE:                 /* Create a file. */
	{
		char *file_name = f->R.rdi;
		int initial_size =  f->R.rsi;
		bool response = create(file_name, initial_size);	
		f->R.rax = response;
		break;
	}
	case SYS_REMOVE:                 /* Delete a file. */ 
	{
		char *file_name = f->R.rdi;
		int response = remove(file_name);		
		f->R.rax = response;
		break;
	}
	case SYS_OPEN:                   /* Open a file. */
	{
		char *file_name = f->R.rdi;
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
		char *buf = f->R.rsi;
		unsigned str_len = f->R.rdx;
		int response = read(fd, buf, str_len);
		f->R.rax = response;
		break;
	}
	case SYS_WRITE:                  /* Write to a file. */  
	{
		char *buf = f->R.rsi;
		int fd = f->R.rdi;
		unsigned str_len = f->R.rdx;
		int response = write(fd, buf, str_len);
		f->R.rax = response;
		break;
	}
	case SYS_SEEK:                   /* Change position in a file. */  {
		int fd = f->R.rdi;
		size_t position = f->R.rsi;
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
	case SYS_MMAP: {
		// printf("rdi:%p , rsi: %d, rdx: %d, rcx: %d,r8: %d\n", f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	}                 
	case SYS_MUNMAP:{
		munmap(f->R.rdi);
		break;
	}                 /* Remove a memory mapping. */
	default:
		thread_exit();
	}

}


void halt(void) {
	power_off();
}

void exit (int status) {
	char *temp;
	struct thread *curr = thread_current();
	*curr->exit_status = status;

	char *name = curr->name;
	strtok_r(name, " ", &temp);
	printf ("%s: exit(%d)\n", name, *curr->exit_status);
	
	thread_exit ();
}

tid_t fork (const char *thread_name, struct intr_frame *tf) {

	is_safe_access(thread_name);
	memcpy(&thread_current()->sc_tf, tf, sizeof(struct intr_frame));

	uint64_t *page = palloc_get_page(PAL_USER); 
	if ( !is_kern_pte(page) && thread_current()->generation > MAX_GENERATION) {
		return -1;
	}
	palloc_free_page(page);

	char *t_name = malloc(sizeof(thread_current()->name));
	strlcpy(t_name, thread_name, sizeof(thread_current()->name));
	
	/* 1. Clone the current process and change name to thread_name */
	// Registers to be cloned: %rbx, %rsp, %rbp and %r12-%r15
	tid_t pid = process_fork(t_name, tf);

	return pid;
}

int wait(tid_t pid) {
	// Wait for the child process
	return process_wait(pid);
}

int exec(const char *cmd_line) {
	is_safe_access(cmd_line);
	char *cmd_copy = malloc(strlen(cmd_line) + 1);
	strlcpy(cmd_copy, cmd_line, strlen(cmd_line) + 1);
	struct child_info *cf = malloc(sizeof(struct child_info));
	cf->file_name = cmd_copy;
	cf->sema = thread_current()->sema;
	cf->exit_status = thread_current()->exit_status;
	tid_t pid =  process_exec(cf);
	// printf("Pid is: %d\n", pid);
	return pid;
}


/* File functions*/

bool create(const char *file_name, unsigned initial_size){
	is_safe_access(file_name);
	if (file_name == NULL || strlen(file_name) == 0) {
		exit(-1);
		return false;
	}
	if (strlen(file_name) > 256) {
		return false;
	}
	lock_acquire(&prcs_lock);
	struct file *file = filesys_open(file_name);
	if (file != NULL) {
		file_close(file);
		lock_release(&prcs_lock);
		return false;
	} 

	bool is_created = filesys_create(file_name, initial_size);
	lock_release(&prcs_lock);

	return is_created;
};

bool remove(const char *file){
	is_safe_access(file);
	lock_acquire(&prcs_lock);
	bool is_closed = filesys_remove(file);
	lock_release(&prcs_lock);
	return is_closed;
};

int open(const char *file_name){
	is_safe_access(file_name);
	lock_acquire(&prcs_lock);
	struct file *file = filesys_open(file_name);
	if (file == NULL) {
		lock_release(&prcs_lock);
		return -1;
	} else {
		int fd = add_new_file_to_fd_list(file);
		lock_release(&prcs_lock);
		return fd;
	}
};

int filesize (int fd){ 
	lock_acquire(&prcs_lock);
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		lock_release(&prcs_lock);
		return 0;
	}	
	int length =  file_length(file);
	lock_release(&prcs_lock);
	return length;
};
int read(int fd, void *buffer, unsigned size){
	char *a = malloc(sizeof(int));
	memcpy (a , buffer, sizeof(int));
	is_safe_access(buffer);
	struct page *page = spt_find_page( &thread_current()->spt, pg_round_down(buffer));
	// printf("Page is: %d", page == NULL);
	if(page != NULL && !page->writable) {
		exit(-1);
	}

	int response = 0;
	if (fd == 0) {
		lock_acquire(&prcs_lock);
		response = input_getc();
		lock_release(&prcs_lock);
	} else {
		lock_acquire(&prcs_lock);
		struct file *fl = get_file_from_fd(fd);
		if (fl == NULL) {
			lock_release(&prcs_lock);
			return -1;
		}
		if (read_size(fl) < size) {
			return - 1;
		}
		int signed_int = size & ((1 << 31) - 1);
		if (signed_int < 0) {
			lock_release(&prcs_lock);
			return -1;
		} else {
			response = file_read(fl, buffer, size);
		}
		lock_release(&prcs_lock);
		return response;
	}
	return response;
};
int write (int fd, void *buffer, unsigned size){
	
	is_safe_access(buffer);
	int response = 0;
	if (fd == 1) {
		lock_acquire(&prcs_lock);
		putbuf(buffer, size);
		lock_release(&prcs_lock);
		return size;
	} else {
		struct file *file = get_file_from_fd(fd);
		if (file == NULL) {
			return 0;
		}
		int offset = size;
		lock_acquire(&prcs_lock);
		int signed_int = size & ((1 << 31) - 1);
		int sign_bit = size & (1 << 31);
		if (sign_bit != 0) {
			// TODO:
		} else {
			response = file_write(file, buffer, size);
		}
		lock_release(&prcs_lock);
		return response;
	}
};
void seek(int fd, size_t position){
	struct file *fl = get_file_from_fd(fd);
	if (fl == NULL) {
		return;
	}
	lock_acquire(&prcs_lock);
	file_seek(fl, position);
	lock_release(&prcs_lock);
};
unsigned tell(int fd){
	lock_acquire(&prcs_lock);
	struct file *file = get_file_from_fd(fd);
	if (file == NULL) {
		lock_release(&prcs_lock);
		return 0;
	}
	int response = file_tell(file);
	lock_release(&prcs_lock);
	if (response < 0 ) {
		response = 0;
	}
	return (unsigned) response;
};

void close(int fd){
	lock_acquire(&prcs_lock);
	struct thread *curr = thread_current();
	for(struct list_elem *e = list_begin(&curr->files); e != list_end(&curr->files); e = list_next(e)) {
		struct file_information *a = list_entry(e, struct file_information, elem);
		if (a->fd == fd) {
			file_close(a->file);
			list_remove(e);
		}
		if (a->fd > fd) {
			break;
		}
	}
	lock_release(&prcs_lock);
};


// File Fd functions
int add_new_file_to_fd_list(struct file *fl) {
	// Create a fd struct
	struct file_information *new_fd_file = malloc(sizeof(struct file_information));
	new_fd_file->fd = fd_counter;
	fd_counter++;
	new_fd_file->file = fl;
	list_push_back(&thread_current()->files, &new_fd_file->elem);
	return new_fd_file->fd;
}

struct file* get_file_from_fd(int fd) {
	struct thread *curr = thread_current();

	for(struct list_elem *e = list_begin(&curr->files); e != list_end(&curr->files); e = list_next(e)) {
		struct file_information *a = list_entry(e, struct file_information, elem);

		if (a->fd == fd) {
			return a->file;
		}
	}
	return NULL;
}