/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *        The President and Fellows of Harvard College.
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
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
        struct addrspace *as;

        as = kmalloc(sizeof(struct addrspace));
        if (as == NULL) {
                return NULL;
        }

        /*
         * Initialize as needed.
         */
        as->num_regions = 0;
        as->first_region = NULL;

        return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
        struct addrspace *newas;

        newas = as_create();
        if (newas==NULL) {
                return ENOMEM;
        }

        /*
         * Write this.
         */

        newas->num_regions = old->num_regions;
        // deep copy, need to copy physical frame and hpt entry as well
        newas->first_region = copy_region(newas, old->first_region);

        *ret = newas;
        return 0;
}

void
as_destroy(struct addrspace *as)
{
        /*
         * Clean up as needed.
         */

        // clean up all the regions, its physical frame and hpt entry
        destroy_all_region(as, as->first_region);

        // free data structure itself
        kfree(as);
}

void
as_activate(void)
{
        /*flush TLB*/
        struct addrspace *as;

        as = proc_getas();
        if (as == NULL) {
                /*
                 * Kernel thread without an address space; leave the
                 * prior address space in place.
                 */
                return;
        }

        /*
         * Write this.
         */

        /* Disable interrupts on this CPU while frobbing the TLB. */
        int spl = splhigh();
        int i;
        // use TLBHI_INVALID(i) is enough, won't cause duplicated entry
        // error, since it is used for flush
        for (i=0; i<NUM_TLB; i++) {
                // vaddr_t entryno = (vaddr_t)as;
                // entryno = entryno & 0x8FFFF;
                // entryno += i;
                // tlb_write(TLBHI_INVALID(entryno), TLBLO_INVALID(), i);
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }

        splx(spl);
}

void
as_deactivate(void)
{
        /*
         * Write this. For many designs it won't need to actually do
         * anything. See proc.c for an explanation of why it (might)
         * be needed.
         */

        // it do the same as as_activate();
        as_activate();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
                 int readable, int writeable, int executable)
{
        /*
         * Write this.
         */

        // KASSERT(as != NULL);
        if(as == NULL) {
            return EINVAL;
        }

        // copy this from dumbvm.c
        size_t npages;

        /* Align the region. First, the base... */
        sz += vaddr & ~(vaddr_t)PAGE_FRAME;
        vaddr &= PAGE_FRAME;
        /* ...and now the length. */
        sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

        npages = sz / PAGE_SIZE;

        struct region * new_region = create_region(vaddr, npages, readable, writeable, executable);
        add_region_to_as(as, new_region);

        // define a new region successfully.
        return 0;
}
    
int
as_prepare_load(struct addrspace *as)
{
        /*
         * Write this.
         */
        // as_activate();
        // make use of prepare_load_recover_flag to recover the
        // access stuff later
        struct region * cur_region = as->first_region;
        while(cur_region != NULL) {
                if(cur_region->is_writeable == 0) {
                        // is_writeable set to 1 or 8 there is no different
                        cur_region->is_writeable = 8;
                        cur_region->prepare_load_recover_flag = true;
                } 

                cur_region = cur_region->next_region;
        }
        
        return 0;
}

int
as_complete_load(struct addrspace *as)
{
        /*
         * Write this.
         */
        // as_activate();

        struct region * cur_region = as->first_region;
        while(cur_region != NULL) {
                if(cur_region->prepare_load_recover_flag) {
                        // KASSERT(cur_region->is_writeable == 1);
                        cur_region->is_writeable = 0;
                        cur_region->prepare_load_recover_flag = false;
                } 

                cur_region = cur_region->next_region;
        }
        return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        /*
         * Write this.
         */
        // USERSTACK - STACK_PAGE_NUMS * PAGE_SIZE is the vbase
        // STACK_PAGE_NUMS * PAGE_SIZE is the size of region
        int as_define_stack_success = 
                as_define_region(as, USERSTACK - STACK_PAGE_NUMS * PAGE_SIZE, STACK_PAGE_NUMS * PAGE_SIZE, 1, 1, 1);

        // KASSERT(as_define_stack_success == 0);
        if (as_define_stack_success == 0) {
                /* Initial user-level stack pointer */
                *stackptr = USERSTACK;

                return 0;
        } else {
                // don't know set to which value is appropriate
                *stackptr = USERSTACK;
                // indicate problems
                return EFAULT;
        }
}

/**
*   Create a new region
*
*   @param vbase the bottom/start of vaddr
*   @param npages decribe page number
*   @param the last three is the access parameter
*   
*   @return created new region
*/
struct region * 
create_region(vaddr_t vbase, size_t npages, int readable, int writeable, int executable) {
        struct region* new_region = (struct region*) kmalloc(sizeof(struct region));
        new_region->vbase = vbase;
        new_region->npages = npages;
        new_region->is_readable = readable;
        new_region->is_writeable = writeable;
        new_region->is_executable = executable;
        new_region->prepare_load_recover_flag = false;
        new_region->next_region = NULL;

        return new_region;
}

/*
*   Add newly created region to addrspace
*/
void
add_region_to_as(struct addrspace* as, struct region* _region) {
        if (as->first_region == NULL) {
                as->first_region = _region;
                return;
        }

        struct region * cur_region = as->first_region;
        while(cur_region->next_region != NULL) {
                cur_region = cur_region->next_region;
        }

        cur_region->next_region = _region;
        as->num_regions++;
}

/* 
*   Deep copy the regions along with the linked-list, copy the corresponding
*   physical frame, and insert newly created frame to hash_page_table. And this
*   is done in a recursive manner, since the underlying data structure is linked-list.
*
*   @param  struct addrspace *  The new address space contains the virtual addr space
*                               Used in hpt_insert
*   @param  struct region *     The original region we copy from
*
*   @return struct region *     The succesfully copied new region
*/
struct region * 
copy_region(struct addrspace * newas, struct region* old_region) {
        if(old_region == NULL) {
                return NULL;
        }

        struct region * new_region = (struct region *)kmalloc(sizeof(struct region));
        new_region->vbase = old_region->vbase;
        new_region->npages = old_region->npages;
        new_region->is_readable = old_region->is_readable;
        new_region->is_writeable = old_region->is_writeable;
        new_region->is_executable = old_region->is_executable;
        new_region->prepare_load_recover_flag = old_region->prepare_load_recover_flag;

        /********* physical frame copy and hpt insertion ***********/ 
        uint32_t i;
        for(i = 0; i<old_region->npages; i++) {
            // THINK ABOUT THAT npages == 3, vbase == 2000, page_size == 1000
            // Need check in virtual addr space [2000-3000], [3000-4000], [4000-5000]
            // copy all the possible physical frames.
            struct hpt_entry * original_hpt_entry = hpt_lookup(proc_getas(),old_region->vbase+i*PAGE_SIZE);

            // If is null, means tha this virtual addr page do not have a physical frame mapping
            // means it hasn't been used.
            if(original_hpt_entry != NULL) {
                void * temp = kmalloc(PAGE_SIZE);
                vaddr_t alloc_vaddr = (vaddr_t) temp;

                // KASSERT(alloc_vaddr != 0);
                if(alloc_vaddr == 0) {
                    return NULL;
                }
                // get PFN of the allocated frame
                paddr_t alloc_paddr = KVADDR_TO_PADDR(alloc_vaddr);
                paddr_t alloc_paddr_PFN = alloc_paddr & PAGE_FRAME;

                paddr_t original_physical_addr = original_hpt_entry->PFN;
                // reset cache/dirty/valid bits
                original_physical_addr &= ~TLBLO_NOCACHE;
                original_physical_addr &= ~TLBLO_DIRTY;
                original_physical_addr &= ~TLBLO_VALID;

                // physical copy, use memmove instead of momcopy since
                // this function will deal with overlapping
                memmove((void *)PADDR_TO_KVADDR(alloc_paddr_PFN), 
                    (const void *)PADDR_TO_KVADDR(original_physical_addr), 
                    PAGE_SIZE);

                // add to hpt_table
                // according to previous example, 
                // base could be 2000, 3000, 4000 in this case
                // so we should use original_hpt_entry->VPN, instead of
                // the vbase of original region.
                hpt_insert(newas, 
                    original_hpt_entry->VPN, 
                    alloc_paddr_PFN, 
                    DEFAULT_CACHE_BIT, 
                    old_region->is_writeable, 
                    DEFAULT_VALID_BIT);
            }
        }

        new_region->next_region = copy_region(newas, old_region->next_region);

        return new_region;
}

/**
*   Destroy physical frame, hpt_entry, and the bookkeeping data structure itself
*   Same style as copy_region, in a recursive manner.
*
*   @param  struct addrspace *  The address space contains the virtual addr space
*                               Used in hpt_lookup
*   @param  struct region *     The original region we copy from
*/
void 
destroy_all_region(struct addrspace* as, struct region* _region) {
        if(as->first_region == NULL) {
                return;
        }

        if(_region == NULL) {
                return;
        }

        destroy_all_region(as, _region->next_region);
        // free all the physical frame        
        uint32_t i;
        for(i = 0; i<_region->npages; i++) {
            // use passed in as instead of proc_getas
            struct hpt_entry * temp_hpt_entry = hpt_lookup(as, _region->vbase+i*PAGE_SIZE);

            if(temp_hpt_entry != NULL) {
                paddr_t original_physical_addr = temp_hpt_entry->PFN;
                // reset cache/dirty/valid bits
                original_physical_addr &= ~TLBLO_NOCACHE;
                original_physical_addr &= ~TLBLO_DIRTY;
                original_physical_addr &= ~TLBLO_VALID;
                // free physical frame
                kfree((void *)PADDR_TO_KVADDR(original_physical_addr));

                // delete corresponding entry in hash_page_table
                hpt_delete(temp_hpt_entry->Pid, temp_hpt_entry->VPN);
            }
        }

        as->num_regions--;
        kfree(_region);
}

/**
*   Used in vm_fault, to get corresponding region which contains the 
*   vaddr.
*
*   @param  struct addrspace *  Current as
*   @param  struct vaddr_t      The virtual address we try to locate
*
*   @return struct region *     The region contains the virtual address
*/
struct region * 
vaddr_region_mapping(struct addrspace* as, vaddr_t fault_addr) {
        if(as->first_region == NULL) {
                return NULL;
        }

        struct region * cur_region = as->first_region;
        while(cur_region != NULL) {
                if (fault_addr >= cur_region->vbase &&
                        fault_addr < cur_region->vbase + cur_region->npages * PAGE_SIZE) {
                        // KASSERT(cur_region->vbase > 0);
                        // KASSERT(cur_region->npages > 0);
                        // KASSERT((cur_region->vbase & PAGE_FRAME) == cur_region->vbase);
                        return cur_region;
                }
                cur_region = cur_region->next_region;
        }
        // no match
        return NULL;
}