#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <synch.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h> 
#include <proc.h>   

/* Place your page table functions here */
// Use the hash function on wiki
uint32_t 
hpt_hash(struct addrspace *as, vaddr_t VPN) {
        uint32_t index;

        index = (((uint32_t)as) ^ VPN) % hpt_size;
        return index;
}

/**
*   Initialization of hash_page_table, use ram_stealmem,
*   put it on the bottom of RAM. Call this before frametable_init.
*/
void
hpt_init() {
    paddr_t top_of_ram = ram_getsize();
    int page_num = top_of_ram / PAGE_SIZE;

    hpt_size = HPT_SIZE_TIMES_LARGE * page_num;

    hash_page_table = (struct hpt_entry *)
        kmalloc(sizeof(struct hpt_entry) * hpt_size);

    int i;
    for(i=0; i<hpt_size; i++) {
        (hash_page_table + i)->Pid = NULL;
        (hash_page_table + i)->VPN = 0;
        (hash_page_table + i)->PFN = 0;
        (hash_page_table + i)->next_entry = NULL;
    }

    hpt_lock = lock_create("hpt_lock");
}

/**
*   Find a match in hash_page_table, every entry is uniquely
*   identified by addrspace pointer(Pid) and virtual page number
*/
struct hpt_entry * 
hpt_lookup(struct addrspace * as, vaddr_t VPN) {
        lock_acquire(hpt_lock);

        uint32_t index = hpt_hash(as, VPN);

        struct hpt_entry * cur_hpt_entry = (hash_page_table + index);
        
        while(cur_hpt_entry != NULL) {
            if(cur_hpt_entry->Pid == as && cur_hpt_entry->VPN == VPN) {
                // check valid bit
                uint32_t validity = cur_hpt_entry->PFN & TLBLO_VALID;
                if(validity == TLBLO_VALID) {
                    lock_release(hpt_lock);
                    return cur_hpt_entry;
                } 
            }
            cur_hpt_entry = cur_hpt_entry->next_entry;
        }

        lock_release(hpt_lock);
        return NULL;
}

/**
*   Insert a new entry into hash_page_table
*
*   @param  struct addrspace *  Used as Pid of in hpt_entry
*   @param  vaddr_t             virtual page number
*   @param  paddr_t             Allocated physical frame on RAM
*   @param  the last three is the access flag bit
*
*   @return hpt_entry *         Newly inserted hpt_entry
*/
struct hpt_entry *
hpt_insert(struct addrspace * as, vaddr_t VPN, paddr_t PFN, int cache_bit, int dirty_bit, int valid_bit) {
        paddr_t PFN_incorporate_bits = PFN;
        if(cache_bit == 1) {
            PFN_incorporate_bits = PFN_incorporate_bits | TLBLO_NOCACHE; 
        }
        if(dirty_bit > 0) {
            PFN_incorporate_bits = PFN_incorporate_bits | TLBLO_DIRTY;
        }
        if(valid_bit == 1) {
            PFN_incorporate_bits = PFN_incorporate_bits | TLBLO_VALID;
        }

        uint32_t index = hpt_hash(as, VPN);

        lock_acquire(hpt_lock);

        struct hpt_entry * cur_hpt_entry = (hash_page_table + index);

        // find the entry that the next_entry field is NULL
        while(cur_hpt_entry->next_entry != NULL) {
            cur_hpt_entry = cur_hpt_entry->next_entry;
        }

        // the very first empty entry or attached to the current entry
        if(cur_hpt_entry->Pid == NULL && cur_hpt_entry->VPN == 0 && cur_hpt_entry->PFN == 0) {
            // write to cur_hpt_entry
            cur_hpt_entry->Pid = as;
            cur_hpt_entry->VPN = VPN;
            cur_hpt_entry->PFN = PFN_incorporate_bits;
            cur_hpt_entry->next_entry = NULL;

            lock_release(hpt_lock);
            return cur_hpt_entry;
        } else {
            // attach to cur_hpt_entry
            int i;
            for (i=0; i<hpt_size; i++) {
                // find an empty slot
                struct hpt_entry * temp_hpt_entry = (hash_page_table + i);
                if(temp_hpt_entry->Pid == NULL && temp_hpt_entry->VPN == 0 && temp_hpt_entry->PFN == 0) {
                    temp_hpt_entry->Pid = as;
                    temp_hpt_entry->VPN = VPN;
                    temp_hpt_entry->PFN = PFN_incorporate_bits;
                    temp_hpt_entry->next_entry = NULL;

                    cur_hpt_entry->next_entry = temp_hpt_entry;
                    
                    lock_release(hpt_lock);
                    return temp_hpt_entry;
                }
            }
        }

        lock_release(hpt_lock);
        // can't inserted an entry then return NULL
        return NULL;
}

/**
*   Find and delete an entry of hash_page_table
*
*   @param  struct addrspace *  Pid
*   @param  vaddr_t             VPN
*
*   @return int                 Indicate success or not
*/
int
hpt_delete(struct addrspace * as, vaddr_t VPN) {
        lock_acquire(hpt_lock);

        uint32_t index = hpt_hash(as, VPN);

        struct hpt_entry * cur_hpt_entry = (hash_page_table + index);
        
        // the entry going to be deleted is in the head/ on the index
        if(cur_hpt_entry->Pid == as && cur_hpt_entry->VPN == VPN) {
            if(cur_hpt_entry->next_entry == NULL) {
                cur_hpt_entry->Pid = NULL;
                cur_hpt_entry->VPN = 0;
                cur_hpt_entry->PFN = 0;
                cur_hpt_entry->next_entry = NULL;

                lock_release(hpt_lock);
                return 0;
            } else {
                // move the next entry to current entry, since it is
                // a chain, but only next entry is enough
                // and we need to clear contents of next_entry
                cur_hpt_entry->Pid = cur_hpt_entry->next_entry->Pid;
                cur_hpt_entry->next_entry->Pid = NULL;
                cur_hpt_entry->VPN = cur_hpt_entry->next_entry->VPN;
                cur_hpt_entry->next_entry->VPN = 0;
                cur_hpt_entry->PFN = cur_hpt_entry->next_entry->PFN;
                cur_hpt_entry->next_entry->PFN = 0;
                struct hpt_entry * temp_hpt_entry = cur_hpt_entry->next_entry->next_entry;
                cur_hpt_entry->next_entry->next_entry = NULL;
                cur_hpt_entry->next_entry = temp_hpt_entry;

                lock_release(hpt_lock);
                return 0;
            }
        }


        // the entry is in the middle/end of the chain
        // and the first entry is inspected previously.
        struct hpt_entry * previous_hpt_entry = cur_hpt_entry;
        cur_hpt_entry = cur_hpt_entry->next_entry;

        while(cur_hpt_entry != NULL) {
            if(cur_hpt_entry->Pid == as && cur_hpt_entry->VPN == VPN) {
                // in the end of the chain
                if(cur_hpt_entry->next_entry == NULL) {
                    cur_hpt_entry->Pid = NULL;
                    cur_hpt_entry->VPN = 0;
                    cur_hpt_entry->PFN = 0;
                    cur_hpt_entry->next_entry = NULL;

                    previous_hpt_entry->next_entry = NULL;

                    lock_release(hpt_lock);
                    return 0;
                } else {
                    // in the middle of the chain
                    // change the pointer of previous chain next_entry.
                    // clear the content of current entry
                    cur_hpt_entry->Pid = NULL;
                    cur_hpt_entry->VPN = 0;
                    cur_hpt_entry->PFN = 0;

                    previous_hpt_entry->next_entry = cur_hpt_entry->next_entry;
                    cur_hpt_entry->next_entry = NULL;

                    lock_release(hpt_lock);
                    return 0;
                }
            }
            previous_hpt_entry = cur_hpt_entry;
            cur_hpt_entry = cur_hpt_entry->next_entry;
        }

        lock_release(hpt_lock);
        // not exist
        return 0;
}

/**
*   Auxiliary function used to write to TLB
*/
void 
write_to_tlb(struct hpt_entry * entry) {
        // entry hi, entyr low
        uint32_t ehi, elo;
        int spl;
        // Disable interrupte when write to TLB
        spl = splhigh();

        ehi = entry->VPN;
        elo = entry->PFN;

        tlb_random(ehi, elo);

        splx(spl);
}

void 
vm_bootstrap(void)
{
        /* Initialise VM sub-system.  You probably want to initialise your 
           frame table here as well.
        */

        // put hpt init before frametable, so it will allocate
        // wil ramsteal, won't be managed by frame_table
        hpt_init();

		frame_table_init();
}

/**
*   Get called every tlb miss. And it's the only function where we allocate
*   physical frame to missed virtual address. Bind virtual address and 
*   physical frame by putting that into hash_page_table.
*/

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        struct addrspace *as;

        // KASSERT(curproc != NULL);
        if (curproc == NULL) {
            return EFAULT;
        }

        as = proc_getas();
        // KASSERT(as != NULL);
        if (as == NULL) {
            /*
             * No address space set up. This is probably also a
             * kernel fault early in boot.
             */
            return EFAULT;
        }

        // transform to VPN
        vaddr_t vir_page_num = faultaddress & PAGE_FRAME;
        // KASSERT(vir_page_num != 0);
        
        /************* check region validity **************/
        struct region* _region = vaddr_region_mapping(as, vir_page_num);
        // KASSERT(_region != NULL);
        if(_region == NULL) {
            return EFAULT;
        }
        
        switch (faulttype) {
            case VM_FAULT_READONLY:
                /* We always create pages read-write, so we can't get this */
                return EFAULT;

            case VM_FAULT_READ:
                // KASSERT(_region->is_readable > 0);
                if (!_region->is_readable) {
                    return EFAULT;
                }
                break;

            case VM_FAULT_WRITE:
                // KASSERT(_region->is_writeable > 0);
                if (!_region->is_writeable) {
                    return EFAULT;
                }
                break;

            default:
                return EINVAL;
        }

        /***** look up hpt to see if there is a valid translation *****/
        struct hpt_entry * lookup_valid_translation_in_hpt = 
            hpt_lookup(as, vir_page_num);

        if(lookup_valid_translation_in_hpt != NULL) {
            // find valid translation, load TLB
            write_to_tlb(lookup_valid_translation_in_hpt);
            return 0;
        }

        /****** allocate frame, zero-fill, insert PTE to hpt ******/
        void * temp = kmalloc(PAGE_SIZE);
        vaddr_t alloc_vaddr = (vaddr_t) temp;

        // KASSERT(alloc_vaddr != 0);
        if(alloc_vaddr == 0) {
            return ENOMEM;
        }

        // get PFN of the allocated frame
        paddr_t alloc_paddr = KVADDR_TO_PADDR(alloc_vaddr);
        paddr_t phy_frame_number = alloc_paddr & TLBLO_PPAGE;
        // KASSERT(phy_frame_number != 0);

        int dirty_bit = _region->is_writeable;

        struct hpt_entry * inserted_hpt_entry = hpt_insert(
            as, 
            vir_page_num, 
            phy_frame_number, 
            DEFAULT_CACHE_BIT,
            dirty_bit, 
            DEFAULT_VALID_BIT);

        // KASSERT(inserted_hpt_entry != NULL);
        if(inserted_hpt_entry == NULL) {
            return ENOMEM;
        } else {
            write_to_tlb(inserted_hpt_entry);
        }
        
        return 0;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

