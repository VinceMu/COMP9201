#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>
#include <spl.h>

/* Place your page table functions here */

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


struct page_table_entry{
        paddr_t frame_addr;
        vaddr_t page_addr;
        struct addrspace *pid;
        int next_entry;
};

void frametable_init(void);
int look_up_page_table(vaddr_t a, struct addrspace *pid);
void page_table_init(void);
int look_up_region(vaddr_t vaddr, struct addrspace *as);
int page_table_insert(vaddr_t v_addr);
struct page_table_entry *page_table=0;





void page_table_init(void){
        struct spinlock page_table_lock = SPINLOCK_INITIALIZER;

        spinlock_acquire(&page_table_lock);

        paddr_t page_table_size = ram_getsize();
        int total_size = (page_table_size / PAGE_SIZE) * 2; /* page table has 2 times of frame table */
        int total_page = total_size * (sizeof(struct page_table_entry) + PAGE_SIZE) / PAGE_SIZE;

        //allocate the space page table needed.
        page_table = (struct page_table_entry*) PADDR_TO_KVADDR(ram_stealmem(total_page));
        if(page_table == 0){
                panic("ERROR page table init.\n");
        }
        spinlock_release(&page_table_lock);
}


/*
 * look up the virtual address in page table,
 *
 * if the address is found, return the physical address.
 * else return 0.
 *
 */
int look_up_page_table(vaddr_t a, struct addrspace *pid){

        int total_frame = ram_getsize() / PAGE_SIZE;
        int index = a % total_frame;
        paddr_t offset = a - PAGE_SIZE * index;

        /* search for valid virtual address */
        while(page_table[index].pid != pid){
                index = page_table[index].next_entry;
                if(index == 0)
                        return -1;
        }

        paddr_t paddr = page_table[index].frame_addr;

        /* update tlb */
        int spl = splhigh();
        uint32_t ehi = a;
        uint32_t elo = paddr + offset;
        tlb_random(ehi, elo);
        splx(spl);

        return 0;
}

/*
 * look up region table
 */

int look_up_region(vaddr_t vaddr, struct addrspace *as){
        if(vaddr < as->as_vbase1 + as->as_npages1*PAGE_SIZE
           && vaddr > as->as_vbase1 ){
                page_table_insert(vaddr);
        }else if(vaddr < as->as_vbase2 + as->as_npages2 * PAGE_SIZE &&
                vaddr > as->as_npages2){
                page_table_insert(vaddr);
        }else{
                return -1;
        }
        return 0;
}


int page_table_insert(vaddr_t v_addr){
        struct spinlock page_table_lock = SPINLOCK_INITIALIZER;
        spinlock_acquire(&page_table_lock);


        int empty_page_table = 0;
        int total_frame = ram_getsize() / PAGE_SIZE;
        int index = v_addr % total_frame;
        struct page_table_entry temp = page_table[index];
        while(temp.next_entry != 0){
                temp = page_table[temp.next_entry];
        }

        /* find empty slot of external linking table */
        int i;
        for(i=total_frame; i < total_frame*2; i++){
                if(page_table[i].frame_addr == 0){
                        empty_page_table = i;
                        break;
                }
        }

        /* do not have empty external page table entry */
        if(i >= total_frame*2 -1){
                return ENOMEM;
        }

        KASSERT(empty_page_table != 0);

        /* insert data into page_table */
        temp.next_entry = empty_page_table;
        page_table[empty_page_table].frame_addr = alloc_kpages(1);
        page_table[empty_page_table].next_entry = 0;
        page_table[empty_page_table].page_addr = v_addr % PAGE_SIZE;

        struct addrspace *as;
        as = proc_getas();

        page_table[empty_page_table].pid = as;
        spinlock_release(&page_table_lock);

        /* update tlb */
        int spl = splhigh();
        uint32_t ehi = v_addr % PAGE_SIZE;
        uint32_t elo = page_table[empty_page_table].frame_addr;
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
        static struct spinlock page_table_lock = SPINLOCK_INITIALIZER;

        spinlock_acquire(&page_table_lock);


        struct addrspace *as;
        as = proc_getas();
        struct addrspace *pid = as->pid;

        /* if tlb miss, search in the page table */
        int err = look_up_page_table(faultaddress, pid);
        if(err != 0){
                /* if not in the page table, look up in the region. */
                err = look_up_region(faultaddress, as);
                if(err != 0) {
                        spinlock_release(&page_table_lock);
                        return EFAULT;
                }
                else{
                        spinlock_release(&page_table_lock);
                        return 0;
                }
        }
        spinlock_release(&page_table_lock);
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

