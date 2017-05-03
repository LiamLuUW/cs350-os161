/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include "opt-A3.h"


/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
#ifdef OPT_A3
struct CoremapEntry *coremap;
unsigned int numofentry = 0;
int offset;
#endif

void
vm_bootstrap(void)
{
	
#ifdef OPT_A3	
	paddr_t pfirst;
	paddr_t plast;
	ram_getsize(&pfirst, &plast); //get first and last address for pyhsical memory

	//kprintf("first address is %x, last is %x\n",pfirst,plast);

	int index = plast / PAGE_SIZE; 
	int index2= pfirst / PAGE_SIZE;
	numofentry = index-index2;

	//kprintf("frist index is %d, last is %d, gap is %d\n", index2, index, index-index2);
	coremap = (struct CoremapEntry *) PADDR_TO_KVADDR(pfirst); //assign coremap to the pfirst

	//int entrySize = sizeof(struct CoremapEntry);
	uint32_t coremapsize = numofentry * sizeof(struct CoremapEntry); //count the total size
	coremapsize =ROUNDUP(coremapsize, PAGE_SIZE);
	//kprintf("coremapsize is %d\n", coremapsize);


	paddr_t temp = pfirst + coremapsize; //temp is the address after coremap constructed
	offset = temp / PAGE_SIZE; //an offset for coremap index
	//kprintf("last address is %x, %d\n",temp, temp/PAGE_SIZE);

	KASSERT(temp < plast);
	for(unsigned int i=0; i<numofentry; i++){
		coremap[i].status=0;
		coremap[i].numofblocks=0;
		coremap[i].addr = pfirst+coremapsize+(i*PAGE_SIZE);
	}

#endif

	
}

static
paddr_t
getppages(unsigned long npages)
{

#ifdef OPT_A3
	paddr_t addr;
	/*case for before coremap*/
	if(numofentry == 0){

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);
	
		spinlock_release(&stealmem_lock);
	}else{
		spinlock_acquire(&stealmem_lock);
		int flag=0; //flag for find the free space index
		unsigned int i=0; //index

		for(; i<numofentry; i++){
		if(coremap[i].status==0){
			for(unsigned int j=0; j<=npages; j++){
				int index=i;
				if(coremap[index].status == 0 && j!=npages){
					continue;
					index++;
				}else if (j==npages){
					flag=1; // avaliable
					break;
				}else{
					break;
				}
			}
			if(flag){break;}
			
		}
		}
		//kprintf("npages is %d, The block set to is %d\n",(int)npages,i);
		addr=coremap[i].addr;

		/*update coremap entries*/
		for(unsigned int j=0; j<npages; j++){
			coremap[i].status=1;
			coremap[i].numofblocks=npages;
			i++;
		}

		//kprintf("set addr to %x\n",addr);
		//addr = 0x800392d0;
		spinlock_release(&stealmem_lock);
	}
	return addr;
#else
	paddr_t addr;
	spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);
	
	spinlock_release(&stealmem_lock);
	return addr;
#endif
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{

	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);

}


void 
free_kpages(vaddr_t addr)
{
#ifdef OPT_A3
	/* nothing - leak the memory. */
	paddr_t paddr = KVADDR_TO_PADDR(addr); //convert vaddr to paddr
	spinlock_acquire(&stealmem_lock);
	//kprintf("%d\n", offset);
	int index = (paddr / PAGE_SIZE)- offset; //get the first index
	int conblock = coremap[index].numofblocks; //conblock the this block's number of elements
	//kprintf("first index is %d\n",index);
	/*free user part*/
	for(int i=0; i< conblock;i++){
		coremap[index].status=0;
		coremap[index].numofblocks=0;
		index++;
	}
	//kprintf("second index is %d\n",index);
/*	//free text segment
	conblock = coremap[index].numofblocks;
	for(int i=0; i< conblock;i++){
		coremap[index].status=0;
		coremap[index].numofblocks=0;
		index++;
	}
	//kprintf("third index is %d\n",index);
	//free data segment
	conblock = coremap[index].numofblocks;
	for(int i=0; i< conblock;i++){
		coremap[index].status=0;
		coremap[index].numofblocks=0;
		index++;
	}
	//kprintf("fourth index is %d\n",index);
	//free text segment
	conblock = coremap[index].numofblocks;
	for(int i=0; i< conblock;i++){
		coremap[index].status=0;
		coremap[index].numofblocks=0;
		index++;
	}
*/
	//KASSERT(coremap[index].status==0);
	spinlock_release(&stealmem_lock);
#else
	(void) addr;
#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

#ifdef OPT_A3
	int is_text = 0; //flag to check a text segament
#endif

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
	    kprintf("dumbvm: WRITE TO READONLY MEMORY\n");
		/* We always create pages read-write, so we can't get this */
	#ifdef OPT_A3
		return EPERM;
	#else
		panic("dumbvm: got VM_FAULT_READONLY\n");
	#endif
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	//KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	//KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	//KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	//KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	//KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	//KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
#ifdef OPT_A3
		//kprintf("seg1 faultaddress is %x\n", faultaddress);
		int found = 0;
		for(unsigned int i=0; i< as->as_npages1; i++){
			if(faultaddress == as->as_pbase1[i].vaddr){
				found=1;
				if(as->as_pbase1[i].valid == 0){
					paddr = getppages(1);
					as->as_pbase1[i].paddr = paddr;
					as->as_pbase1[i].valid=1;
					break;
				}else{
					paddr = as->as_pbase1[i].paddr;
					break;
				}

			}
		}
		//kprintf("seg1 paddr is %x\n", paddr);
		if(!found){
			kprintf("EORROR PAFE TABLE 1 MISS\n");
		}


#else
		paddr = (faultaddress - vbase1) + as->as_pbase1;
#endif

#ifdef OPT_A3
		is_text=1;
#endif

	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
#ifdef OPT_A3
		int found = 0;
		for(unsigned int i=0; i< as->as_npages2; i++){
			if(faultaddress == as->as_pbase2[i].vaddr){
				found=1;
				if(as->as_pbase2[i].valid == 0){
					paddr = getppages(1);
					as->as_pbase2[i].paddr = paddr;
					as->as_pbase2[i].valid=1;
					break;
				}else{
					paddr = as->as_pbase2[i].paddr;
					break;
				}

			}
		}
		//kprintf("seg2 paddr is %x\n", paddr);
		if(!found){
			kprintf("EORROR PAFE TABLE 2 MISS\n");
		}


#else
		paddr = (faultaddress - vbase2) + as->as_pbase2;
#endif

	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
//#ifdef OPT_A3
		//kprintf("stackbase is %x, TOP IS %x\n", stackbase, stacktop);		
		/*kprintf("stack faultaddress is %x\n", faultaddress);
		int found = 0;
		for(unsigned int i=0; i< DUMBVM_STACKPAGES; i++){
			if(faultaddress == as->as_stackpt[i].vaddr){
				found=1;
				if(as->as_stackpt[i].valid == 0){
					paddr = getppages(1);
					as->as_stackpt[i].paddr = paddr;
					as->as_stackpt[i].valid=1;
					break;
				}else{
					paddr = as->as_stackpt[i].paddr;
					break;
				}

			}
		}
		kprintf("it paddr is %x\n", paddr);
		if(!found){
			kprintf("EORROR PAFE TABLE 3 MISS\n");
		}


#else */
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
//#endif
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	//KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
#ifdef OPT_A3
		if(is_text && as->load_finished){
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			elo &= ~TLBLO_DIRTY;
			//kprintf("I am a text\n");
		}else{
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			//kprintf("I am NOT a test\n");
		}
#endif
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

#ifdef OPT_A3
	int duplicate = 0 ;
	uint32_t temphi, templo;
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&temphi, &templo, i);
		ehi = faultaddress;
		if(is_text && as->load_finished){
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			elo &= ~TLBLO_DIRTY;
			//kprintf("I am a text\n");
		}else{
			elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
			//kprintf("I am NOT a test\n");
		}
		if (temphi == ehi) {
			duplicate = 1; //there is a duplicate
			break;
		}
	}
	if(!duplicate){
		tlb_random(ehi,elo);
	}else{
		kprintf("There is a duplicate\n");
	}
	splx(spl);
	return 0;
#endif
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	//splx(spl);
	//return EFAULT;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
#ifdef OPT_A3
	as->load_finished = 0; //set load flag to false

#endif
	return as;
}

void
as_destroy(struct addrspace *as)
{
#ifdef OPT_A3
	/*
	free_kpages(PADDR_TO_KVADDR(as->as_pbase1));
	free_kpages(PADDR_TO_KVADDR(as->as_pbase2));
	*/
	kfree(as->as_pbase1);

	//kprintf("seg1 freeed\n");
	kfree(as->as_pbase2);

	//kprintf("seg2 freeed\n");
	vaddr_t temp_stack = PADDR_TO_KVADDR(as->as_stackpbase);
	free_kpages(temp_stack);
	//kfree(as->as_stackpt);
	//kprintf("seg3 freeed\n");
#endif
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;
#ifdef OPT_A3
	// FOR PAGE TABLE 
	struct p_t_entry *PageTable = kmalloc(npages * (sizeof (struct p_t_entry))); //page table for text segnment
	//int offset = 12; //size page size is 4096
	for(unsigned int i=0; i<npages; i++){
		PageTable[i].vaddr = vaddr + (i * PAGE_SIZE);
		PageTable[i].valid = 0;
	}
	//kprintf("I AM BEING CALLED\n");
#endif

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		as->as_pbase1 = PageTable;
		//kprintf("%x\n", (vaddr_t)PageTable);
		//kprintf("1: %x\n", (vaddr_t) as->as_pbase1);
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		as->as_pbase2 =  PageTable;
		//kprintf("2: %x\n",(vaddr_t) as->as_pbase2);
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

/*
static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}
*/
int
as_prepare_load(struct addrspace *as)
{
	//KASSERT(as->as_pbase1 == 0);
	//KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

/*	kprintf("npages1: %d\n",as->as_npages1);

#ifdef OPT_A3	
	for(unsigned int i=0; i<as->as_npages1; i++){
		kprintf("Corresponding vaddr is %x\n",as->as_pbase1[i].vaddr);
		as->as_pbase1[0].paddr = getppages(1);
	}
#else
	as->as_pbase1 = getppages(as->as_npages1);
#endif
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}


	kprintf("npages2: %d\n",as->as_npages2);
#ifdef OPT_A3	
	for(unsigned int i=0; i<as->as_npages2; i++){
		as->as_pbase2[0].paddr = getppages(1);
	}
#else
	as->as_pbase2 = getppages(as->as_npages2);
#endif
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}
	kprintf("npagesstack: %d\n",DUMBVM_STACKPAGES);
#ifdef OPT_A3

#else
	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
#endif
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	
	as_zero_region((paddr_t)as->as_pbase1, as->as_npages1);
	as_zero_region((paddr_t)as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
*/
	return 0;

}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
#ifdef OPT_A3
	as->load_finished=1;
#endif
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	//KASSERT(as->as_stackpbase != 0);
	(void) as;
	*stackptr = USERSTACK;

	/*int sizeofentry = (sizeof (struct p_t_entry));
	struct p_t_entry *stacktemp = kmalloc(DUMBVM_STACKPAGES *sizeofentry);	//initialize the stack page table

	for(int i=0; i< DUMBVM_STACKPAGES; i++){
		stacktemp[i].vaddr = (vaddr_t) (USERSTACK - i * PAGE_SIZE );
		kprintf("%x\n", stacktemp[i].vaddr );
		stacktemp[i].valid = 0;
	}

	//kprintf("first is %x\n",stacktemp[0].vaddr);
	//kprintf("last is %x\n",stacktemp[11].vaddr);
	as->as_stackpt = stacktemp;
*/
	
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;
#ifdef OPT_A3
	int size = (sizeof (struct p_t_entry));
	new->load_finished = old->load_finished;
	new->as_pbase1 = kmalloc((old->as_npages1) *size ); //page table for text segnment
	//int offset = 12; //size page size is 4096
	for(unsigned int i=0; i<(old->as_npages1); i++){
		new->as_pbase1[i].vaddr = old->as_pbase1[i].vaddr;
		new->as_pbase1[i].paddr = old->as_pbase1[i].paddr;
		new->as_pbase1[i].valid = old->as_pbase1[i].valid;
	
	}
	new->as_pbase2 = kmalloc((old->as_npages2) * size); //page table for text segnment
	//int offset = 12; //size page size is 4096
	for(unsigned int i=0; i<(old->as_npages2); i++){
		new->as_pbase2[i].vaddr = old->as_pbase2[i].vaddr;
		new->as_pbase2[i].paddr = old->as_pbase2[i].paddr;
		new->as_pbase2[i].valid = old->as_pbase2[i].valid;
	}
	/*new->as_stackpt = kmalloc(12 * size); //page table for text segnment
	//int offset = 12; //size page size is 4096
	for(unsigned int i=0; i<(12); i++){
		new->as_stackpt[i].vaddr = old->as_stackpt[i].vaddr;
		new->as_stackpt[i].paddr = old->as_stackpt[i].paddr;
		new->as_stackpt[i].valid = old->as_stackpt[i].valid;
	}	*/
#else

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}
#endif

#ifdef OPT_A3
		memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
	
	*ret = new;
	return 0;
#else
	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
	
	*ret = new;
	return 0;
#endif
}
