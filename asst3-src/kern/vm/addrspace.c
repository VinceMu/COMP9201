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
#define USER_STACKPAGES 16

void destroy_region(struct addrspace* as, struct region* region);

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

        as->first_region = NULL;
        as->num_regions = 0;
        as->as_stackpbase = 0;
        as->pid = as;
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
        struct region* old_temp = old->first_region;
        struct region* new_temp = newas->first_region;

        while(old_temp != NULL){
                as_define_region(newas, old_temp->vbase, old_temp->npages * PAGE_SIZE,
                                 old_temp->read, old_temp->write,old_temp->exe);
                old_temp = old_temp->next;
                new_temp = new_temp->next;

                memmove((void *)new_temp->vbase,
                        (void *)old_temp->vbase,
                        PAGE_SIZE*old_temp->npages);
        }


        *ret = newas;
        return 0;
}


void destroy_region(struct addrspace* as, struct region* region){
        if (region != NULL) {
                as->num_regions--;
                destroy_region(as, region->next);
                kfree(region);
        }
}

void
as_destroy(struct addrspace *as)
{
        /*
         * Clean up as needed.
         */
        destroy_region(as, as->first_region);


        as->first_region = NULL;
        as->num_regions = 0;
        as->as_stackpbase = 0;
        as->pid = 0;

        kfree(as);
}

void
as_activate(void)
{
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
        int i, spl;
        spl = splhigh();
        for (i=0; i<NUM_TLB; i++) {
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

        as_activate();
}


static
void
as_zero_region(vaddr_t vaddr, int npages)
{
        bzero((void *) vaddr, npages * PAGE_SIZE);
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
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
                 int readable, int writeable, int executable)
{
        if(as == NULL){
                return ENOSYS;
        }


        struct region* new_region = (struct region*) kmalloc(sizeof(struct region));


        new_region->vbase = vaddr - vaddr % PAGE_SIZE;
        new_region->npages = (memsize + vaddr % PAGE_SIZE + PAGE_SIZE - 1 ) / PAGE_SIZE ;
        new_region->read = readable;
        new_region->exe = executable;
        new_region->write = writeable;
        new_region->next = NULL;

     //   size_t npages;

        if(as->first_region == NULL){
                as->first_region = new_region;
                as->num_regions++;
                return 0;
        }

        struct region* temp_region = as->first_region;

        /* find last region in region list */
        while(temp_region->next != NULL){
                temp_region = temp_region->next;
        }

        temp_region->next = new_region;
        as->num_regions++;

        return 0;
}



int
as_prepare_load(struct addrspace *as)
{
        /*
         * Write this.
         */

        struct region *temp = as->first_region;
        while(temp->next != NULL){
                as_zero_region(temp->vbase, temp->npages);
                temp = temp->next;
        }
        return 0;
}

int
as_complete_load(struct addrspace *as)
{
        /*
         * Write this.
         */

        (void)as;
        return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        /*
         * Write this.
         */

        as_define_region(as, USERSTACK - USER_STACKPAGES * PAGE_SIZE , USER_STACKPAGES * PAGE_SIZE, 1, 1, 1);
        /* Initial user-level stack pointer */
        *stackptr = USERSTACK;

        return 0;
}

