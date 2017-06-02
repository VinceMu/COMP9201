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
struct page_table_entry *page_table=0;
int total_size;

void page_table_init(void){
        struct spinlock page_table_lock = SPINLOCK_INITIALIZER;

        spinlock_acquire(&page_table_lock);

        paddr_t page_table_size = ram_getsize();

        total_size = (page_table_size / PAGE_SIZE) * 2; /* the number of page table is 2 times more than physical frames */
        int total_page = total_size * sizeof(struct page_table_entry) / PAGE_SIZE + 1;

        //allocate the space page table needed.
        page_table = (struct page_table_entry*) PADDR_TO_KVADDR(ram_stealmem(total_page));
        if(page_table == 0){
                panic("ERROR page table init.\n");
        }

        for(int i=0; i< total_size; i++){
                page_table[i].frame_addr = 0;
                page_table[i].next_entry = 0;
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

        int index = a / total_size;

        /* search for valid virtual address */
        while(page_table[index].pid != pid || page_table[index].page_addr != (a - a % PAGE_SIZE)){
                index = page_table[index].next_entry;
                /* can not find valid page entry */
                if(index == 0)
                        return -1;
        }

        paddr_t paddr = page_table[index].frame_addr;

        /* if current page table entry does not valid */
        if(paddr == 0 || page_table[index].pid != pid)
                return -1;

//        struct spinlock tlb_locker = SPINLOCK_INITIALIZER;
//        spinlock_acquire(&tlb_locker);

        /* update tlb */
        int spl = splhigh();
        uint32_t ehi = a & TLBHI_VPAGE;
        uint32_t elo = KVADDR_TO_PADDR(paddr)| TLBLO_DIRTY | TLBLO_VALID;

        tlb_random(ehi, elo | TLBLO_VALID | TLBLO_DIRTY);
        splx(spl);
//        spinlock_release(&tlb_locker);

        return 0;
}

/*
 * look up region table
 */

int look_up_region(vaddr_t vaddr, struct addrspace *as){


        if(as->first_region->next != NULL){
                if(vaddr >= as->first_region->vbase && vaddr <= as->first_region->vbase + as->first_region->npages * PAGE_SIZE ){
                        page_table_insert(vaddr);
                        return 0;
                }
        }

        struct region *temp = as->first_region->next;
        while(temp != 0){
                if(vaddr >= temp->vbase && vaddr <= temp->vbase + temp->npages * PAGE_SIZE ){
                        page_table_insert(vaddr);
                        return 0;
                }
                temp = temp->next;
        }
        return -1;
}


int page_table_insert(vaddr_t v_addr){


        int empty_page_table = 0;
        int total_frame = total_size / 2;
        int index = v_addr / total_frame;
        while(page_table[index].next_entry != 0){
                index = page_table[index].next_entry;
        }


        /* find empty slot of external linking table */

        int i;
        for(i=total_frame; i < total_frame*2 ; i++){
                if(page_table[i].frame_addr == 0){
                        empty_page_table = i;
                        break;
                }
        }

        /* i out of range */
        if(i >= total_frame*2 -1){
                return ENOMEM;
        }



        KASSERT(empty_page_table != 0);

        /* insert data into page_table */
        page_table[index].next_entry = empty_page_table;
        if(index < total_frame){
                empty_page_table = index;
        }
        page_table[empty_page_table].frame_addr = alloc_kpages(1);
        page_table[empty_page_table].next_entry = 0;
        page_table[empty_page_table].page_addr = v_addr - v_addr % PAGE_SIZE;

        struct addrspace *as;
        as = proc_getas();

//        struct spinlock tlb_locker = SPINLOCK_INITIALIZER;
//        spinlock_acquire      (&tlb_locker);

        /* update tlb */
        int spl = splhigh();
        uint32_t ehi = v_addr & TLBHI_VPAGE;
        uint32_t elo = KVADDR_TO_PADDR(page_table[empty_page_table].frame_addr)| TLBLO_DIRTY | TLBLO_VALID;
        tlb_random(ehi, elo | TLBLO_VALID | TLBLO_DIRTY);
        splx(spl);
        page_table[empty_page_table].pid = as;
//        spinlock_release(&tlb_locker);

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

