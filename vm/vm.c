/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/mmu.h"
#include <stdio.h>
#include "userprog/process.h"
#include "string.h"

#define STACK_SIZE (1 << 20)
#define	PHYS_BASE ((void *) LOADER_PHYS_BASE)
#define LOADER_PHYS_BASE 0x200000


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	ASSERT(spt);
	
	// // printf("Upage addrs: %p\n", upage);
	// printf("Find page stage: %d\n", hash_size(&spt->pages));
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initializer according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = calloc(1, sizeof(struct page));
		// printf("Found a page\n");

		switch(type)
		{
			// case (VM_ANON + VM_MARKER_0):
			case VM_ANON:
				uninit_new(page, upage, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				PANIC("VM_FILE vm_alloc_page_with_initializer");
				break;
			case VM_PAGE_CACHE:
				PANIC("VM_PAGE_CACHE vm_alloc_page_with_initializer");
				break;
			default:
				break;
		}
		page->writable = writable;
		page->type = type;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page (spt, page);
	}
err:
	// printf("Returned false\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *temp_entry = calloc(1, sizeof(struct page));
	temp_entry->va = va;
	struct hash_elem *e = hash_find(&spt->pages,&temp_entry->elem);
	if (e==NULL){
		return NULL;
	}
	return hash_entry(e, struct page, elem);;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt ,
		struct page *page ) {
	/* TODO: Fill this function. */
	ASSERT(page);
	struct hash_elem *e = hash_insert(&spt->pages, &page->elem);
	return  e== NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	// Beware this function needs careful synchronization
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// pml4_clear_page
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	ASSERT(frame);
	void *kpage = palloc_get_page(PAL_USER);
	
	if(kpage==NULL){
		PANIC("get_frame_fail");
		return vm_evict_frame();
	}

	frame->kva = kpage;
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static bool
vm_stack_growth (void *addr UNUSED) {
	// printf("stack growth %p \n", addr);
	// ASSERT(false);
	struct frame *frame = vm_get_frame ();
	ASSERT(frame);
	/* Add the page to the process's address space. */
	if (!pml4_set_page (thread_current ()->pml4, pg_round_down (addr), frame->kva, true))
	{	
		palloc_free_page (frame->kva);
		return false;
	}
	// printf("vm_stack_growth finished ### \n");
	return true;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}


bool is_valid(void *ptr) {
	if (ptr == NULL || is_kernel_vaddr(ptr) || pml4_get_page(thread_current()->pml4, ptr) == NULL) {
		return false;
	}
	return true;
}
/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user UNUSED, bool write UNUSED, bool not_present) {
	struct supplemental_page_table *spt  = &thread_current ()->spt;
	struct page *page = calloc(1, sizeof(struct page));
	uint64_t fault_addr = pg_round_down(addr);
	void *rsp = user ? f->rsp :thread_current ()->rsp;

	if((addr == NULL || !not_present) ||  addr >= KERN_BASE ){
		// // printf("invalid address: null %d, not_present %d, LOADER_KERN_BASE %d, addr higher than kernbase %d \n",fault_addr == NULL,!not_present, addr >= KERN_BASE);
		return false;
	}
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// allocate in the stack
	page = spt_find_page (spt, fault_addr);
	

	uint64_t fd_addr = (uint64_t) addr;
	uint64_t diff = USER_STACK - fault_addr;
	// printf("User: %lx, Kernel: %lX, fault_addr: %lX, diff: %lx\n", PHYS_BASE, KERN_BASE, fd_addr, diff);

	if(page != NULL) {
		// !!! Need to check whether page is not claimed yet !!!
		return vm_do_claim_page(page);
	} else if (page == NULL && fd_addr >= (f->rsp - PGSIZE) && (PHYS_BASE - fault_addr <= STACK_SIZE)) {
		return vm_stack_growth (fault_addr);
	} else {
		return false;
	}
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va ) {
	struct page *page = calloc(1, sizeof(struct page));
	struct supplemental_page_table *spt  = &thread_current ()->spt;
	/* TODO: Fill this function */
	page = spt_find_page(spt, va);
	ASSERT(page != NULL);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	ASSERT(frame);
	// // printf("vm_do_claim_page\n");
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	ASSERT(thread_current()->pml4);
	// // printf("Thread id: va page: %d, %p\n", thread_current()->tid, page->va);
	// printf("Thread id: va page: %d, %d\n", thread_current()->tid, pml4_get_page (thread_current()->pml4, page->va) == NULL);

	// uint64_t *mmu = pml4_create();
	bool success = (pml4_get_page (thread_current()->pml4, page->va) == NULL
			&& pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable));
	// // printf("pml4_set_page is %d\n",success);
	if (!success){
		return false;
	}
	page->type = page_get_type(page);
	ASSERT(page->type);
	// // printf("claim success\n");
	return swap_in (page, frame->kva);
}

// Hash Functions required for [frame_map]. Uses 'kaddr' as key.
static uint64_t
spte_hash_func(const struct hash_elem *eleme, void *aux UNUSED)
{
  struct page *entry = hash_entry(eleme, struct page, elem);
  ASSERT(entry);
  return hash_int( (uint64_t)entry->va );
}
static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct page *a_entry = hash_entry(a, struct page, elem);
  struct page *b_entry = hash_entry(b, struct page, elem);
  ASSERT(a_entry);
  ASSERT(b_entry);
  // printf("Less checke\n");
  // printf("Less and up: %p, %p\n", a_entry->va, b_entry->va);
  return a_entry->va < b_entry->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	// // printf("spt initialize \n");
	ASSERT(spt);
	hash_init(&spt->pages,spte_hash_func, spte_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	struct hash_iterator iter;
	hash_first (&iter, &src->pages);
	while(hash_next(&iter)) {
		struct page *page = hash_entry(hash_cur(&iter), struct page, elem);
		struct page *new_page =calloc(1, sizeof(struct page));

		new_page->writable = page->writable;
		new_page->type = page->type;
		new_page->operations = page->operations;
		new_page->uninit = page->uninit;
		new_page->va = page->va;
		// new_page->uninit.init = NULL;

		switch (page->operations->type)
		{
		case VM_UNINIT:
			switch (page->type)
			{
			case VM_ANON:
				new_page->uninit.aux = malloc(sizeof(struct lazy_aux));
				memcpy(new_page->uninit.aux, page->uninit.aux, sizeof(struct lazy_aux));
				break;
			
			default:
				NOT_REACHED();
				break;
			}
			// // printf("Vm uninit in spt copy\n");
			// swap_in(new_page, new_page->va);
			if(!spt_insert_page(dst, new_page)) {
				return false;
			}
			break;
		default:
			{
				bool success = vm_do_claim_page(new_page);
				memcpy(new_page->frame->kva, page->frame->kva, PGSIZE);
				success = spt_insert_page(dst, new_page);
				
				if (!success) {
					return success;
				}
				break;
			}
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void destroy_entry (struct hash_elem *e, void *aux) {
	struct page *page = hash_entry(e, struct page, elem);
	vm_dealloc_page(page);
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (hash_size(&spt->pages) == 0) {
		return;
	}

	hash_destroy(&spt->pages, destroy_entry);
	supplemental_page_table_init(&spt->pages);
}


