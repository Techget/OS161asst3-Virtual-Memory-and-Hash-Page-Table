#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>

/* Place your frametable data-structures here 
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

// need a LOCK for frame table operation
static struct spinlock frame_table_lock = SPINLOCK_INITIALIZER;

struct frame_table_entry {
        paddr_t corresponding_paddr;
        struct frame_table_entry * next_free;
        bool in_use_flag;
};

struct frame_table {
        struct frame_table_entry * frame_table_arr;
        // point to the lowest free_frame in frame_table_arr,
        // used in finding the next free frame
        struct frame_table_entry * lowest_free_frame_entry;
        // before is the frames allocated to kernel and frame table itself
        int free_ram_frame_start_index;
        // total frame/physical page there will be
        int page_number;
};

struct frame_table * ft_table = 0;

// put this init function in vm_bootstrap() 
void frame_table_init() {
        paddr_t top_of_ram = ram_getsize();
        struct frame_table * ft_table_temp;
        /* use existing bump allocator to allocate frame table and hash table */
        ft_table_temp = (struct frame_table *)kmalloc(sizeof(struct frame_table));
        ft_table_temp->page_number = top_of_ram / PAGE_SIZE;
        ft_table_temp->frame_table_arr = (struct frame_table_entry *)
                kmalloc(sizeof(struct frame_table_entry) * ft_table_temp->page_number);

        /* Then use ram_getfirstfree() to figure out how much memory have been
        * used including frame table, 
        */
        paddr_t free_ram_start = ram_getfirstfree();
        ft_table_temp->free_ram_frame_start_index = free_ram_start / PAGE_SIZE;
        ft_table_temp->lowest_free_frame_entry = &(ft_table_temp->frame_table_arr[ft_table_temp->free_ram_frame_start_index]);

        /* and then initialize the frame table, then start use frame table based
        * allocator
        */
        int i;
        for (i=0; i<ft_table_temp->page_number; i++ ) {
                if (i < ft_table_temp->free_ram_frame_start_index) {
                        ft_table_temp->frame_table_arr[i].in_use_flag = true;
                        ft_table_temp->frame_table_arr[i].next_free = NULL;
                } else {
                        ft_table_temp->frame_table_arr[i].in_use_flag = false;
                        if (i == ft_table_temp->page_number - 1) {
                                ft_table_temp->frame_table_arr[i].next_free = NULL; 
                        } else {
                                ft_table_temp->frame_table_arr[i].next_free = &(ft_table_temp->frame_table_arr[i+1]);
                        }
                }
                ft_table_temp->frame_table_arr[i].corresponding_paddr = i * PAGE_SIZE;
        }

        ft_table = ft_table_temp;
}       

/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

vaddr_t alloc_kpages(unsigned int npages)
{
        /*
         * IMPLEMENT ME.  You should replace this code with a proper
         *                implementation.
         */
        // KASSERT(npages == 1);

        if (ft_table == 0) {
                /* user ram_stealmem */
                paddr_t addr;

                spinlock_acquire(&stealmem_lock);
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);

                if(addr == 0)
                        return 0;

                return PADDR_TO_KVADDR(addr);
        } else {
                /* use my allocator as frame table is now initialized */
                if (ft_table->lowest_free_frame_entry == NULL) {
                        // means ram is fully filled
                        return 0;
                }

                KASSERT(ft_table->lowest_free_frame_entry->in_use_flag == false);
                if (ft_table->lowest_free_frame_entry->in_use_flag == true) {
                        // try to allocate an already allocated frame
                        return 0;
                }

                spinlock_acquire(&frame_table_lock);

                ft_table->lowest_free_frame_entry->in_use_flag = true;
                paddr_t ret_addr = ft_table->lowest_free_frame_entry->corresponding_paddr;
                struct frame_table_entry * temp = ft_table->lowest_free_frame_entry;
                ft_table->lowest_free_frame_entry = ft_table->lowest_free_frame_entry->next_free;
                temp->next_free = NULL;

                // zero-out allocated physical frame
                // this haven't been tested
                bzero((void *)PADDR_TO_KVADDR(ret_addr), npages * PAGE_SIZE);

                spinlock_release(&frame_table_lock);

                return PADDR_TO_KVADDR(ret_addr);
        }        
}

void free_kpages(vaddr_t addr)
{
        // addr should be within KSEG0
        KASSERT(addr >= MIPS_KSEG0 && addr< MIPS_KSEG1 );
        if (addr < MIPS_KSEG0 || addr >= MIPS_KSEG1) {
                return;
        }

        paddr_t physical_addr = KVADDR_TO_PADDR(addr);

        int frame_number = physical_addr >> 12;

        KASSERT(frame_number >= ft_table->free_ram_frame_start_index);
        if (frame_number < ft_table->free_ram_frame_start_index) {
                // try to free parts allocated by bump allocator
                return;
        }

        KASSERT(ft_table->frame_table_arr[frame_number].in_use_flag == true);
        if (ft_table->frame_table_arr[frame_number].in_use_flag == false) {
                // try to free an already freed frame 
                return;
        }

        spinlock_acquire(&frame_table_lock);

        if (ft_table->lowest_free_frame_entry == NULL) {
                ft_table->frame_table_arr[frame_number].in_use_flag = false;
                ft_table->lowest_free_frame_entry = &(ft_table->frame_table_arr[frame_number]);
                ft_table->frame_table_arr[frame_number].next_free = NULL;
                return;
        }

        if (&(ft_table->frame_table_arr[frame_number]) < ft_table->lowest_free_frame_entry) {
                // under the head
                ft_table->frame_table_arr[frame_number].next_free = ft_table->lowest_free_frame_entry;
                ft_table->lowest_free_frame_entry = &(ft_table->frame_table_arr[frame_number]);
                ft_table->frame_table_arr[frame_number].in_use_flag = false;
        } else {
                ft_table->frame_table_arr[frame_number].in_use_flag = false;
                // trace back to the begining of the ft_table->frame_table_arr
                int i = frame_number - 1;
                for (; i>=ft_table->free_ram_frame_start_index; i--) {
                        if (ft_table->frame_table_arr[i].in_use_flag == false) {
                                ft_table->frame_table_arr[i].next_free = &(ft_table->frame_table_arr[frame_number]);
                                break;
                        }
                }
                KASSERT(i >= ft_table->free_ram_frame_start_index);

                // trace forward to the end of ft_table->frame_table_arr
                i = frame_number + 1;
                for (; i < ft_table->page_number; i++) {
                        if (ft_table->frame_table_arr[i].in_use_flag == false) {
                                ft_table->frame_table_arr[frame_number].next_free = &(ft_table->frame_table_arr[i]);
                                break;
                        }
                }

                if (i == ft_table->page_number) {
                        ft_table->frame_table_arr[frame_number].next_free = NULL;
                }
        }

        spinlock_release(&frame_table_lock);
}

