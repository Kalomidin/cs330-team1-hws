#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/malloc.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);


static struct list addrs;
	

/* General process initializer for initd and other process. */
static void
process_init (void) {
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;
	
	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	
	struct child_info *child_info = palloc_get_page(0);

	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	child_info->file_name = fn_copy;
	child_info->sema = palloc_get_page(0);
	sema_init(child_info->sema, 0);
	if(thread_current()->exit_status == NULL) {
		child_info->exit_status = palloc_get_page(0);
	} else {
		child_info->exit_status = thread_current()->exit_status;
	}

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, child_info);
	if (tid == TID_ERROR) {
		palloc_free_page (fn_copy);
		return tid;
	} else {
		child_info->pid = tid;
		struct thread *t = thread_current();
		list_push_back(&t->children, &child_info->elem);
		return tid;
	}
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	memcpy(&thread_current()->tf, if_, sizeof(struct intr_frame));

	tid_t pid = thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());

	// Add child to the list
	struct child_info *cf = palloc_get_page(0);

	// cf->file_name = t_name;
	cf->sema = malloc(sizeof(struct semaphore));
	sema_init(cf->sema, 0);
	cf->exit_status = malloc(sizeof(int));
	cf->pid = pid;

	list_push_back(&thread_current()->children, &cf->elem);
	sema_up(&fork_sema);

	sema_down(cf->sema);
	return pid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kern_pte(pte)) {
		return true;
	}

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER);

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	 
	memcpy(newpage, parent_page, PGSIZE);
	// printf("Parent %p, child %p \n", newpage, parent_page);

	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	sema_down(&fork_sema);
	

	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */

	struct intr_frame *parent_if = &parent->tf;

	// struct intr_frame *parent_if = malloc(sizeof(struct intr_frame));
	memcpy(parent_if, &parent->tf, sizeof(struct intr_frame));


	// printf("Current name: %s, %p, %p, (rdi): %p, (rip): %p, (parent rip): %p, (tid): %d\n", current->name, if_.rsp, if_.R.rbp, if_.R.rdi, if_.rip, parent->tf.rip, current->tid);

	// /* 1. Read the cpu context to local stack. */
	memcpy (&if_, &parent->sc_tf, sizeof (struct intr_frame));


	for(struct list_elem *e = list_begin(&parent->children); e != list_end(&parent->children); e = list_next(e)) {
		struct child_info *child_info = list_entry(e, struct child_info, elem);
		if (current->tid == child_info->pid) {
			current->sema = child_info->sema;
			current->exit_status = child_info->exit_status;
			break;
		}
	}


	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);

#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else

	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	for(struct list_elem *e = list_begin(&parent->files); e != list_end(&parent->files); e = list_next(e)) {
		struct file_information *file_info = list_entry(e, struct file_information, elem);
		struct  file_information *cpy_file_info = malloc(sizeof(struct file_information));
		cpy_file_info->fd = file_info->fd;
		cpy_file_info->file = file_duplicate(file_info->file); 
		list_push_back(&current->files, &cpy_file_info->elem);
	}

	process_init ();
	
	/* Finally, switch to the newly created process. */
	
	// // Registers to be cloned: %rbx, %rsp, %rbp and %r12-%r15
	// if_.R.rdi = 0;
	// if_.R.rsi = 0;
	// if_.R.rcx = 0;
	if_.R.rax = 0;

	sema_up(current->sema);
	do_iret (&if_);
	return;
error:
	sema_up(current->sema);
	exit (-1);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
	struct child_info *child_info = f_name;
	char *file_name = child_info->file_name;
	
	struct thread *curr = thread_current();

	curr->sema = child_info->sema;
	curr->exit_status = child_info->exit_status;

	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup ();

	/* And then load the binary */
	success = load (file_name, &_if);

	/* If load failed, quit. */

	// Remove filename if it is in kernel 
	if (is_user_vaddr(file_name)) {
		palloc_free_page (file_name);
	}

	if (!success)
		return -1;

	/* Start switched process. */
	do_iret (&_if);


	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

	struct thread *curr = thread_current();

	for(struct list_elem *e = list_begin(&curr->children); e != list_end(&curr->children); e = list_next(e)) {
		struct child_info *child_info = list_entry(e, struct child_info, elem);
		if (child_info->pid == child_tid) {
			sema_down(child_info->sema);
			list_remove(e);
			return *child_info->exit_status;
		}
	}

	return -1;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	
	if (curr->sema != NULL) {
		if (curr->file != NULL) {
			file_close(curr->file);
		}
		// Remove and close all the files
		for(struct list_elem *e = list_begin(&curr->files); e != list_end(&curr->files);) {
			struct file_information *file_info = list_entry(e, struct file_information, elem);
			close(file_info->fd);
			list_remove(e);
			e = list_next(e);
		}

		sema_up(curr->sema);
	}
	process_cleanup ();
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);

	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;


	/* Allocate and activate page directory. */
	printf("Hello world1");

	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	printf("Hello world2");
	process_activate (thread_current ());
	printf("Hello world3");

	char *cpy_filename = malloc(strlen(file_name) + 1);
	strlcpy(cpy_filename, file_name, strlen(file_name) + 1);
	char *temp;
	strtok_r(cpy_filename, " ", &temp);
	/* Open executable file. */
	lock_acquire(&prcs_lock);
	file = filesys_open (cpy_filename);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	file_deny_write(file);
	thread_current()->file = file;

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}
	printf("Hello world4\n");
	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;
		printf("Hello world6\n");

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);
		printf("Hello world7\n");
		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		printf("Hello world8\n");
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				printf("Hello world9\n");
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					printf("Hello world1000\n");
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
								{
									printf("Hello world10\n");
									goto done;
								}
				}
				else{
printf("Hello world11\n");
					goto done;
				}
					
				break;
		}
	}
	printf("Hello world5\n");

	/* Set up stack. */

	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */
	 // #1 breaking the string
	

   	char *token, *save_ptr;
	list_init(&addrs);
	
	//Break into words
	for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
		*token += '\0';
		if_->rsp -= (strlen(token) + 1);
		struct pointer_elem *p = malloc(sizeof(struct pointer_elem));
		p->pointer_address = if_->rsp;
		list_push_front(&addrs, &p->elem);
		memcpy(&if_->rsp, token, strlen(token));
		// char *buffer = if_->rsp;
	 }
 
	// Word alignment
	int word_align = (if_->rsp) % sizeof(char*);
	if_->rsp -= word_align;
	memset(if_->rsp, 0, word_align);
	if_->rsp -= sizeof(char *);
	memset(if_->rsp, 0, sizeof(char *));


	// Refer addrs of data
	struct list_elem *e;
	e = list_begin(&addrs);
	// struct pointer_elem *addr_fn = list_entry(e, struct pointer_elem, elem);
	int argc = list_size(&addrs);

	while(e!=list_end(&addrs)){
		struct pointer_elem *addr = list_entry(e, struct pointer_elem, elem);
		if_->rsp -=  sizeof(char *);
		//printf("ppointer %p, %s\n",addr->pointer_address, (addr->pointer_address));
		memcpy(if_->rsp, &addr->pointer_address, sizeof(char *));
		e=list_next(e);
	}

	if_->R.rsi = if_->rsp;
	if_->R.rdi = argc;
	if_->rsp -= sizeof(void (*) ());
	memset(if_->rsp, 0, sizeof(void (*) () ));
	
	success = true;

done:
	lock_release(&prcs_lock);
	/* We arrive here whether the load is successful or not. */
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	printf("palloc hulu-\n");
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);
	printf("palloc --\n");
	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		printf("palloc 0\n");
		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;
		printf("palloc 1\n");
		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		printf("palloc 2\n");
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}
		printf("palloc 3\n");
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	printf("lazy load segment\n");
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct lazy_aux *lazy_aux = (struct segment *) aux;
		/* Load this page. */
	size_t page_read_bytes = lazy_aux->page_read_bytes;

	file_seek(lazy_aux->file, lazy_aux->ofs);
	if (file_read (lazy_aux->file, page->frame->kva, page_read_bytes) != (int) page_read_bytes) {
		palloc_free_page (page->kpage);
		return false;
	}
	printf("palloc 2\n");
	memset (page->frame->kva + page_read_bytes, 0, lazy_aux->page_zero_bytes);
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	printf("Real Hello\n");
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct lazy_aux *aux = malloc(sizeof(struct lazy_aux));
		aux->file = file;
		aux->ofs = ofs;
		aux->page_read_bytes= page_read_bytes;
		aux->page_zero_bytes=page_zero_bytes;

		printf("Real Hello2\n");
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;
		printf("Real Hello3\n");
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */


	// kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	// if (kpage != NULL) {
	// 	success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
	// 	if (success)
	// 		if_->rsp = USER_STACK;
	// 	else
	// 		palloc_free_page (kpage);
	// }
	return success;
}
#endif /* VM */
