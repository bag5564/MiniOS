/*
 * kernel_code.c - Project 2, CMPSC 473
 * Copyright 2021 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <kernel.h>
#include <types.h>
#include <printf.h>
#include <malloc.h>
#include <string.h>

void *page_table = NULL; /* TODO: Must be initialized to the page table address */
void *user_stack = NULL; /* TODO: Must be initialized to a user stack virtual address */
void *user_program = NULL; /* TODO: Must be initialized to a user program virtual address */

typedef unsigned long long u64;

// structure used to populate Level 1 entries
struct page_pte {
	u64 present:1; 		// bit P
	u64 writable:1;		// bit R/W
	u64 user_mode:1;	// bit U/S
	u64 otherbits:9;
	u64 page_address:40;	// physical address
	u64 avail:7;		// reserved
	u64 pke:4;		// MPK/MPE
	u64 nonexecute:1;
};
// structure used to populate Level 2, 3, or 4 entries
struct page_pde {
	u64 present:1;		// bit P
	u64 writable:1;		// bit R/W
	u64 user_mode:1;	// bit U/S
	u64 otherbits:9;
	u64 page_address:40;	// physical address
	u64 avail:11;		// reserved
	u64 nonexecute:1;
};

void kernel_init(void *ustack, void *uprogram, void *memory, size_t memorySize)
{
	// 'memory' points to the place where memory can be used to create
	// page tables (assume 1:1 initial virtual-to-physical mappings here)
	// 'memorySize' is the maximum allowed size, do not exceed that (given just in case)

	// TODO: Create a page table here
	// LEVEL 1 tables (2048 tables = 1048576 entries) - PTE 
	struct page_pte *p = (struct page_pte *) memory;
	printf("p %p\n",p);
	for (int i = 0; i < 1048576; i++) {
		p[i].present = 1;
		p[i].writable = 1;
		p[i].user_mode = 0;
		p[i].otherbits = 0;
		p[i].page_address = i;
		p[i].avail = 0;
		p[i].pke = 0;
		p[i].nonexecute = 0;
	}
	// LEVEL 2 TABLES (4 tables = 2048 entries) - PDE
	struct page_pde *pd = (struct page_pde *) (p + 1048576);
	printf("pde %p\n",pd);
	for (int j = 0; j <2048; j++) {
		struct page_pte *start_pte = p + 512 * j;
		pd[j].present = 1;
		pd[j].writable = 1;
		pd[j].user_mode = 0;
		pd[j].otherbits = 0;
		pd[j].page_address = (u64) start_pte >> 12; // PTE page address
		pd[j].avail = 0;
		pd[j].nonexecute = 0;
	}
	// LEVEL 3 TABLE (1 table = 512 entries) - PDPE
	struct page_pde *pdpe = (struct page_pde *) (pd + 2048);
	printf("pdpe %p\n",pdpe);
	// initialize 4 entries in table
	for (int k = 0; k < 4; k++) {
		struct page_pde *start_pde = pd + 512 * k;
		pdpe[k].present = 1;
		pdpe[k].writable = 1;
		pdpe[k].user_mode = 0;
		pdpe[k].otherbits = 0;
		pdpe[k].page_address = (u64) start_pde >> 12; // PDE page address
		pdpe[k].avail = 0;
		pdpe[k].nonexecute = 0;
	}
	// LEVEL 4 TABLE (1 table = 512 entries) - PMLE4E
        struct page_pde *pmle4e = (struct page_pde *) (pdpe + 512);
	printf("pmle4e %p\n", pmle4e);
	// initialize 1 entry in table
	struct page_pde *start_pdpe = pdpe;
	pmle4e[0].present = 1;
	pmle4e[0].writable = 1;
	pmle4e[0].user_mode = 0;
	pmle4e[0].otherbits = 0;
	pmle4e[0].page_address = (u64) start_pdpe >> 12; // PDPE page address
	pmle4e[0].avail = 0;
	pmle4e[0].nonexecute = 0;
	
	// initialize page table address
	page_table = pmle4e;

	// TODO: It is OK for Q1-Q3, but should changed
	// for the final part's Q4 when creating your own user page table
	user_stack = ustack;

	// TODO: It is OK for Q1-Q3, but should changed
	// for the final part's Q4 when creating your own user page table
	user_program = uprogram;

	// The remaining portion just loads the page table,
	// this does not need to be changed:
	// load 'page_table' into the CR3 register
	const char *err = load_page_table(page_table);
	if (err != NULL) {
		printf("ERROR: %s\n", err);
	}

	// The extra credit assignment
	mem_extra_test();
}

/* The entry point for all system calls */
long syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
	// TODO: the system call handler to print a message (n = 1)
	// the system call number is in 'n', make sure it is valid!

	// Arguments are passed in a1,.., a5 and can be of any type
	// (including pointers, which are casted to 'long')
	// For the project, we only use a1 which will contain the address
	// of a string, cast it to a pointer appropriately 

	// For simplicity, assume that the address supplied by the
	// user program is correct
	//
	// Hint: see how 'printf' is used above, you want something very
	// similar here

	if (n==1){
		printf("%s\n", (char*) a1);
		return 0;
	}
	return -1; /* Success: 0, Failure: -1 */
}
