#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");
	// char *c = "Hello from sys handler\n";
	// putbuf(c, 24);
	// check_user_ptr(f->rsp,0);
	// printf("pointer addr special %d, %p\n", &f->rsp, f->rsp);
	// int *p = f->rsp;
	// int syscall_num = *p;
	// p++;
	// int fd = *p;
	// p++;
	// int bf = *p;
	// void *buffer = (void *) bf;
	// p++;
	// int sz = *p;
	// unsigned size = (unsigned) sz;
	// // printf("sys_call %d, %d, %d\n", syscall_num, fd, bf);
	// // printf("buffer %s\n", (char *) buffer);
	// if (syscall_num == SYS_WRITE) {
	// 	write(fd, buffer, size);
	// }
	thread_exit ();
}

_Bool create(const char *file, unsigned initial_size){return true;};
_Bool remove(const char *file){return true;};
int open(const char *file){return -1;};
int filesize (int fd){return -1;};
int read(int fd, void *buffer, unsigned size){return -1;};
int write (int fd, void *buffer, unsigned size){
	if (fd == 1) {
	putbuf(buffer, size);
	} else {
		// TODO
	}

	return 1;
};
void seek(int fd, unsigned position){return;};
unsigned tell(int fd){return 1;};
void close(int fd){return;};