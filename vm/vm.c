/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/mmu.h"
#include <stdio.h>



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
	printf("page_get_type\n");
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
	printf("vm_alloc_page_with_initializer\n");
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initializer according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = malloc(sizeof(struct page));
		page->writable = writable;
		page->type = type;
		printf("vm_type is %d\n",type);
		switch(type)
		{
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

		
		printf("Hello again1\n");
		/* TODO: Insert the page into the spt. */
		return spt_insert_page (spt, page);
	}
err:
	printf("Hello again2\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *temp_entry = malloc(sizeof(struct page));
	temp_entry->va = va;
	struct hash_elem *e = hash_find(&spt->pages,&temp_entry->elem);
	if (e==NULL){
		printf("find failed \n");
		return NULL;
	}
	struct page *page = hash_entry(e, struct page, elem);
	printf("find succeeded \n");
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt ,
		struct page *page ) {
	printf("spt_insert_page\n");
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_insert(&spt->pages, &page->elem);
	printf("inserted %d\n", e== NULL);
	return  e== NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	printf("spt_remove_page\n");
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

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
	printf("PAL_USER is %d\n",PAL_USER);
	void *kpage = palloc_get_page(PAL_USER);
	
	if(kpage==NULL){
		return vm_evict_frame();
	}

	frame->kva = kpage;
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}


/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = malloc(sizeof(struct page));
	printf("Hello world VM %p\n", addr);

	// if(!user){
	// 	return false;
	// }
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// allocate in the stack
	page = spt_find_page (spt, addr);
	printf("page find in fault handle %d\n", page == NULL);
	return vm_do_claim_page (page);
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
	printf("vm_claim_page\n");
	struct page *page = malloc(sizeof(struct page));
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
	printf("vm_do_claim_page\n");
	/* Set links */
	frame->page = page;
	page->frame = frame;

	printf("Set links -------------- %d\n", thread_current()->pml4);
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	
	// uint64_t *mmu = pml4_create();
	bool success = (pml4_get_page (thread_current()->pml4, page->va) == NULL
			&& pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable));
	printf("pml4_set_page is %d\n",success);
	if (!success){
		return false;
	}
	page->type = page_get_type(page);
	printf("claim success\n");
	return swap_in (page, frame->kva);
}

// Hash Functions required for [frame_map]. Uses 'kaddr' as key.
static uint64_t
spte_hash_func(const struct hash_elem *eleme, void *aux UNUSED)
{
  struct page *entry = hash_entry(eleme, struct page, elem);
  return hash_int( (uint64_t)entry->va );
}
static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct page *a_entry = hash_entry(a, struct page, elem);
  struct page *b_entry = hash_entry(b, struct page, elem);
  return a_entry->va < b_entry->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	printf("spt initialize \n");
	hash_init(&spt->pages,spte_hash_func, spte_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
