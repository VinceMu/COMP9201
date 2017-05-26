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
struct frame_table_entry{
    struct addrspace* as;
    paddr_t p_addr;                //physical address
    vaddr_t v_addr;                //virtual address
    bool valid;                 //valid bit
    bool write;                 //writable bit
    struct frame_table_entry *next_empty; //link list record next available entry.
};


struct frame_table_entry *first_avalibe;        /* first empty element of frame table */

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
bool init = false;      /* set true if frame table is inited */
int total_pages;

/* frametable initialisation.
 *                 physical memory
 *   0xA000 0000    ______________
 *                 |              |
 *                 |              |
 *                 |              |
 *                 |______________|<--- first frametable entry's physical address.
 *                 |______________|<--- frametable is here.
 *                 | OS161 kernel |
 *   0x0000 0000   |______________|
 *
 *   1. first get the location where the frame table should be placed.
 *              call ram_getfirstfree();
 *
 *   2. calculate how much pages should be use, and allocate exact number of
 *      space for the frame table entries.
 *              num of pages = total_physical_memory_size / each_page_size
 *
 *   3. initialized each entry.
 */

void frametable_init(void) {

        /* get the size of total ram */
        spinlock_acquire(&stealmem_lock);

        paddr_t first_empty;    /* frametable position */
        paddr_t total_size;     /* total size of physical memory */
        total_size = ram_getsize();
        first_empty = ram_getfirstfree();

        frame_table = (struct frame_table_entry*) PADDR_TO_KVADDR(first_empty);

        total_pages = total_size / PAGE_SIZE;

        /* initialized all frame table entries */
        for(int i=0; i<total_pages; i++){

                if(i < (first_empty / PAGE_SIZE)){
                        frame_table[i].valid = false;
                        frame_table[i].write = true;
                } else {
                        frame_table[i].valid = true;
                        frame_table[i].write = true;

                        /* init the linked list */
                        if(i != total_pages - 1)
                                frame_table[i].next_empty = &frame_table[i+1];
                        //TODO: how to deal with the last frame? What should the next_empty be set?
                        else
                                frame_table[i].next_empty = 0;
                }

                frame_table[i].p_addr = start_pointer + i * PAGE_SIZE;
        }

        first_avalibe = &frame_table[int(first_empty/PAGE_SIZE)];

        init = true;
        spinlock_release(&stealmem_lock);
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

        paddr_t addr;
        spinlock_acquire(&stealmem_lock);

        if(!init){
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);
        } else {
                addr = first_avalibe;
                first_avalibe = first_avalibe->next_empty;
                first_avalibe->write = true;
                first_avalibe->valid = false;
                spinlock_release(&stealmem_lock);
        }

        if(addr == 0)
                return 0;

        return PADDR_TO_KVADDR(addr);
}

void free_kpages(vaddr_t addr)
{
        paddr_t temp = KVADDR_TO_PADDR(addr);
        int i = temp / PAGE_SIZE;       /* get the index of frame table */

        if(i >= total_pages)
                return;

        frame_table[i]->vaild = true;
        frame_table[i]->write = true;
        frame_table[i]->next_empty = first_avalibe;
        first_avalibe = *frame_table[i];

}

