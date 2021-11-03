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

	// CREATE PAGE TABLE
	printf("memory size %zu\n", memorySize);
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
	// initialize extra entries in table
	for (int k = 4; k < 512; k++) {
		pdpe[k].present = 0;
		pdpe[k].writable = 0;
		pdpe[k].user_mode = 0;
		pdpe[k].otherbits = 0;
		pdpe[k].page_address = 0; 
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
	//initialize extra entries in table
	for (int n = 1; n < 512; n++) {
		pmle4e[n].present = 0;
		pmle4e[n].writable = 0;
		pmle4e[n].user_mode = 0;
		pmle4e[n].otherbits = 0;
		pmle4e[n].page_address = 0;
		pmle4e[n].avail = 0;
		pmle4e[n].nonexecute = 0;
	}

	// initialize page table address
	page_table = pmle4e;

	printf("last entry in pmle4e %p\n", (pmle4e + 511));

	// CREATE USER SPACE SUPPORT 
	// addresses for user program and user stack
	void * phys_user_stack = ustack;
	void * phys_user_program = uprogram;
	printf("phys user stack %p\n", phys_user_stack);
	printf("corrected phys user stack %p\n", (phys_user_stack - 4096));	
	printf("phys user program %p\n", phys_user_program);

	// selected virtual addresses for user stack and user program (correspond to last entry in levels 4, 3 and 2 tables)
	void* v_user_stack   = (void *) 0xFFFFFFFFFFE00000; // entry 0 in user level 1 table
	void* v_user_program = (void *) 0xFFFFFFFFFFFFF000; // entry 511 in user level 1 table
        //struct page_pte *address2 = address1 + 512;
	//struct page_pte *address3 = address2 + 512;	
	
	// LEVEL 1 user table (1 table = 512 entries) - user PTE 
	struct page_pte *u_p = (struct page_pte *) (pmle4e + 512);
	printf("u p %p\n", u_p);
	//last entry in u pte
	printf("u p 511 %p\n", (u_p + 511));
	// first initialize all entries with zeroes
	for (int i = 0; i < 512; i++) {
		u_p[i].present = 0;
		u_p[i].writable = 0;
		u_p[i].user_mode = 0;
		u_p[i].otherbits = 0;
		u_p[i].page_address = 0;
		u_p[i].avail = 0;
		u_p[i].pke = 0;
		u_p[i].nonexecute = 0;
	}	
	// initialize user stack entry in level 1 user table
	u_p[0].present = 1;
	u_p[0].writable = 1;
	u_p[0].user_mode = 1;
	u_p[0].otherbits = 0;
	u_p[0].page_address = (u64) (phys_user_stack - 4096) >> 12; // user program page address
	u_p[0].avail = 0;
	u_p[0].pke = 0;
	u_p[0].nonexecute = 0;
	printf("p[0] page_address %p\n", (u64 *) u_p[0].page_address);
	// initialize user program entry in level 1 user table
	u_p[511].present = 1;
	u_p[511].writable = 1;
	u_p[511].user_mode = 1;
	u_p[511].otherbits = 0;
	u_p[511].page_address = (u64) phys_user_program >> 12; // user stack page address (pointing to next page)
	u_p[511].avail = 0;
	u_p[511].pke = 0;
	u_p[511].nonexecute = 0;
	printf("p[511] page_address %p\n", (u64 *) u_p[511].page_address);

	// LEVEL 2 user table (1 table = 512 entries) - user PDE
	struct page_pde *u_pd = (struct page_pde *) (u_p + 512);
	printf("u pde %p\n",u_pd);
	printf("u pde 511 %p\n", (u_pd + 511));
	// initialize first 511 entries with zeroes
	for (int j = 0; j < 511; j++) {
		u_pd[j].present = 0;
		u_pd[j].writable = 0;
		u_pd[j].user_mode = 0;
		u_pd[j].otherbits = 0;
		u_pd[j].page_address = 0;
		u_pd[j].avail = 0;
		u_pd[j].nonexecute = 0;
	}	
	// initialize last entry in table
	//struct page_pte *start_u_pte = u_p;
	u_pd[511].present = 1;
	u_pd[511].writable = 1;
	u_pd[511].user_mode = 1;
	u_pd[511].otherbits = 0;
	u_pd[511].page_address = (u64) u_p >> 12; // user level 1 (PTE) page address
	u_pd[511].avail = 0;
	u_pd[511].nonexecute = 0;	
	printf("pd[511] page_address %p\n", (u64 *) u_pd[511].page_address);

	// LEVEL 3 user table (1 table = 512 entries) - user PDPE
	struct page_pde *u_pdpe = (struct page_pde *) (u_pd + 512);
	printf("u pdpe %p\n",u_pdpe);
	printf("u pdpe 511 %p\n", (u_pdpe + 511));
	// initialize first 511 entries with zeroes
 	for (int k = 0; k < 511; k++) {
		u_pdpe[k].present = 0;
		u_pdpe[k].writable = 0;
		u_pdpe[k].user_mode = 0;
		u_pdpe[k].otherbits = 0;
		u_pdpe[k].page_address = 0;
		u_pdpe[k].avail = 0;
		u_pdpe[k].nonexecute = 0;
	}	
	// initialize last entry in table
	//struct page_pde *start_u_pde = u_pd;
	u_pdpe[511].present = 1;
	u_pdpe[511].writable = 1;
	u_pdpe[511].user_mode = 1;
	u_pdpe[511].otherbits = 0;
	u_pdpe[511].page_address = (u64) u_pd >> 12; // user level 2 (PDE) page address
	u_pdpe[511].avail = 0;
	u_pdpe[511].nonexecute = 0;	
	printf("pdpe[511] page_address %p\n", (u64 *) u_pdpe[511].page_address);

	// create entry in level 4 table
	//struct page_pde *start_pdpe = pdpe;
	pmle4e[511].present = 1;
	pmle4e[511].writable = 1;
	pmle4e[511].user_mode = 1;
	pmle4e[511].otherbits = 0;
	pmle4e[511].page_address = (u64) u_pdpe >> 12; // user level 3 (PDPE) page address
	pmle4e[511].avail = 0;
	pmle4e[511].nonexecute = 0;
	printf("pmle4e[511] page_address %p\n", (u64 *) pmle4e[511].page_address);
	
	user_stack = v_user_stack + 4096;
	user_program = v_user_program;

	
	printf("virtual user stack %p\n", v_user_stack);
	printf("virtual user stack %p\n", (v_user_stack + 4096));
	printf("virtual user program %p\n", v_user_program);


	struct page_pde *f_free = (struct page_pde *) (u_pdpe + 512);
	printf("first free %p\n", f_free);
	// TODO: It is OK for Q1-Q3, but should changed
	// for the final part's Q4 when creating your own user page table
	//user_stack = ustack;

	// TODO: It is OK for Q1-Q3, but should changed
	// for the final part's Q4 when creating your own user page table
	//user_program = uprogram;

	printf("ustack %p\n", ustack);
	printf("uprogram %p\n", uprogram);

	// The remaining portion just loads the page table,
	// this does not need to be changed:
	// load 'page_table' into the CR3 register
	const char *err = load_page_table(page_table);
	if (err != NULL) {
		printf("ERROR: %s\n", err);
	}

	// The extra credit assignment
	mem_extra_test();
	printf("after extra test\n");
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
	printf("in sys call \n");
	if (n==1){
		printf("%s\n", (char*) a1);
		return 0;
	}
	return -1; /* Success: 0, Failure: -1 */
}
