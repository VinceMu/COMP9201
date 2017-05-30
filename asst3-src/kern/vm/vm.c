#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */
/* the pointer of first page table entry in memory */

/*
 * page table is a hashed list. the number of entries is 2
 * times of physical frames. Half of them is use for hashed index,
 * another half is used to deal will hash collision.
 *
 *  _____________             _______________
 * |             |           |               |
 * | hashed index|  ----->   |physical frame |
 * |_____________|           |_______________|
 * |             |
 * |external link|
 * |_____________|
 *
 * page table entry:
 *     __________________________________________________________
 *    | frame address | page address | PID | index of next entry |
 *     ——————————————————————————————————————————————————————————
 */
paddr_t look_up_page_table(vaddr_t a, unint32_t pid);
int page_table_init(void);


struct page_table_entry{
        paddr_t frame_addr;
        vaddr_t page_addr;
        pid_t pid;
        int next_entry;
};

struct page_table_entry *page_table=0;


static struct spinlock page_table_lock = SPINLOCK_INITIALIZER;


void page_table_init(void){
        /* allocate 4 pages memory for page table */
        spinlock_acquire(&stealmem_lock);

        paddr_t page_table_size = ram_getsize();
        int total_size = (page_table_size / PAGE_SIZE) * 2; /* page table has 2 times of frame table */
        int total_page = (total_page * sizeof(struct page_table_entry) + PAGE_SIZE) / PAGE_SIZE;

        /* set the beginning of external link */
        empty_page_table = total_page/2;

        page_table = ram_stealmem(total_page); //allocate the space page table needed.
        if(page_table == 0){
                panic("ERROR page table init.\n");
        }
        spinlock_release(&stealmem_lock);
}


/*
 * look up the virtual address in page table,
 *
 * if the address is found, return the physical address.
 * else return 0.
 *
 */
paddr_t look_up_page_table(vaddr_t a, unint32_t pid){
        int total_frame = get_ramsize() / PAGE_SIZE;
        int index = a % total_frame;
        paddr_t offset = a - PAGE_SIZE * index;

        /* search for valid virtual address */
        while(page_table[index]->pid != pid){
                index = page_table[index].next_entry;
                if(index == 0)
                        return 0;
        }

        paddr_t paddr = page_table[index].frame_addr;

        /* update tlb */
        spl = splhigh();
        ehi = faultaddress;
        elo = paddr;
        tlb_random(ehi, elo);
        splx(spl);

        return paddr + offset;
}

int page_table_insert(vaddr_t v_addr, paddr_t p_addr){
        int empty_page_table;
        int total_frame = get_ramsize() / PAGE_SIZE;
        int index = a % total_frame;
        struct page_table_entry temp = page_table[index];
        while(temp.next_entry != 0){
                temp = page_table[temp.next_entry];
        }

        /* find empty slot of external linking table */
        for(int i=total_frame; i < total_frame*2; i++){
                if(page_table[i].frame_addr == 0){
                        empty_page_table = i;
                        break;
                }
        }

        /* do not have empty external page table entry */
        if(i >= total_frame*2 -1){
                return ENOMEM;
        }

        /* insert data into page_table */
        temp.next_entry = empty_page_table;
        page_table[empty_page_table].frame_addr = alloc_kpages(1);
        page_table[empty_page_table].next_entry = 0;
        page_table[empty_page_table].page_addr = v_addr % PAGE_SIZE;
        page_table[empty_page_table].pid = pid;

        /* update tlb */
        spl = splhigh();
        ehi = v_addr % PAGE_SIZE;
        elo = page_table[empty_page_table].frame_addr;
        tlb_random(ehi, elo);
        splx(spl);
        return 0;
}

void vm_bootstrap(void)
{
        /* Initialise VM sub-system.  You probably want to initialise your 
           frame table here as well.
        */
        page_table_init();
        frametable_init();

}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        if(faulttype == VM_FAULT_READONLY){
                panic("vm_fault: read only.\n");
                return EFAULT;
        }


        struct addrspace *as;
        as = proc_getas();
        uint32_t pid = as->pid;

        /* if tlb miss, search in the page table */
        paddr_t p_addr = look_up_page_table(faultaddress, pid);
        if(p_addr == 0){
                /* if not in the page table, look up in the region. */
                p_addr = look_up_region(faultaddress, as);
                if(p_addr == 0)
                        return EFAULT;
                else
                        return 0;
        } else
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

