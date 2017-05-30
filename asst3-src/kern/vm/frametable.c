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
    int  next_empty; //link list record next available entry.
};


struct frame_table_entry *frame_table=0;       /* frame table linked list */
int first_empty;                           /* first empty index of frame table */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
int total_pages;

/* frametable initialisation.
 *                 physical memory
 *   0xA000 0000    ______________
 *                 |              |
 *                 |              |
 *                 |______________|
 *                 |______________|<--- frametable and first free pointer is here.
 *                 |______________|<--- page table is here.
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

//        kprintf("START, init frametable.\n");
        /* get the size of total ram */
        spinlock_acquire(&stealmem_lock);

        paddr_t first_empty_pointer;    /* frametable position */
        paddr_t total_size;     /* total size of physical memory */
        total_size = ram_getsize();

        first_empty_pointer = ram_getfirstfree();

        frame_table = (struct frame_table_entry*) PADDR_TO_KVADDR(first_empty_pointer);

        total_pages = total_size / PAGE_SIZE;

        /* initialized all frame table entries */
        for(int i=0; i<total_pages; i++){

                if(i < (int)(first_empty_pointer / PAGE_SIZE)){
                        frame_table[i].valid = false;
                        frame_table[i].write = true;
                } else {
                        frame_table[i].valid = true;
                        frame_table[i].write = true;

                        /* init the linked list */
                        if(i != total_pages - 1)
                                frame_table[i].next_empty = i + 1;
                        //TODO: how to deal with the last frame? What should the next_empty be set?
                        else
                                frame_table[i].next_empty = -1;
                }

                frame_table[i].p_addr = i * PAGE_SIZE;
        }


        if(first_empty >= total_pages){
                panic("NO MORE MEMORY\n");
        }

        spinlock_release(&stealmem_lock);

//        kprintf("frame init end\n");
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
//        kprintf("alloc pages\n");
        paddr_t addr;
        spinlock_acquire(&stealmem_lock);

        if(frame_table == 0){   /* if frame table does not init */
                addr = ram_stealmem(npages);
        } else {
                if(first_empty >= total_pages){
                        spinlock_release(&stealmem_lock);
                        panic("ERROR empty frame number.\n");
                }

                //TODO: check if this frame can be used(valid is true).

                addr = frame_table[first_empty].p_addr;

                frame_table[first_empty].write = true;
                frame_table[first_empty].valid = false;
                first_empty = frame_table[first_empty].next_empty;
//                for(int i = 0; i < total_pages; i++){
//                        kprintf("%d, %d\n", i, frame_table[i].next_empty);
//                }
//                kprintf("%d, %d\n", first_empty, frame_table[first_empty].next_empty);

        }
        spinlock_release(&stealmem_lock);

        if(addr == 0)
                return 0;

        return PADDR_TO_KVADDR(addr);
}

void free_kpages(vaddr_t addr)
{
//        kprintf("free k pages\n");
        paddr_t temp = KVADDR_TO_PADDR(addr);
        int i = temp / PAGE_SIZE;       /* get the index of frame table */

        if(i >= total_pages)
                panic("ERROR FREE ADDR.\n");

        frame_table[i].valid = true;
        frame_table[i].write = true;
        frame_table[i].next_empty = first_empty;
        first_empty = i;

}

