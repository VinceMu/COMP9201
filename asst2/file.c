#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */
int filetable_init (struct thread* nt){
    for (int i = 0; i < 3; ++i){
        struct vnode* vn;
        char* fname = kstrdup ("con:");
        if (vfs_open(fname, i?O_WRONLY:O_RDONLY, 0, &vn)){
            kfree (fname);
            return EINVAL;
        }
    
        void* ptr= kmalloc (sizeof (struct fdesc)); 
        if (!ptr){
            vfs_close (vn);
            kfree (fname);
            return EINVAL;
        }
        nt -> fdtable[i] = ptr;
        strcpy (nt->fdtable[i]->file_name,fname);
        kfree (fname);
        *(nt -> fdtable[i]) = (struct fdesc)
        {
            .vn = vn,
            .flags = i? O_WRONLY:O_RDONLY,
            .offset = 0,
            .refcount = 1,
            .lk = lock_create(fname)
        };
    }
    return 0;
}

int sys_open(const char *filename, int flags, mode_t mode, int *retval) {

    //kprintf("Inside OPEN\n");

    int result=0, index = 3;
    struct vnode *vn;
    char *kbuf;
    size_t len;
    kbuf = (char *) kmalloc(sizeof(char)*PATH_MAX);
    result = copyinstr((const_userptr_t)filename,kbuf, PATH_MAX, &len);
    if(result) {
        //kprintf("OPEN- copyinstr failed- %d\n",result);
        kfree(kbuf);
        return result;
    }

    while (curthread->fdtable[index] != NULL ) {
        index++;
    }

    if(index >= OPEN_MAX) {
        //kprintf("OPEN- index>128\n");
        kfree(kbuf);
        return ENFILE;
    }

    if(flags>2 || flags<0){
        kfree(kbuf);
        return EINVAL;
    }


    curthread->fdtable[index] = (struct fdesc *)kmalloc(sizeof(struct fdesc*));
    if(curthread->fdtable[index] == NULL) {
        //kprintf("OPEN- filetable at given index is null\n");
        kfree(kbuf);
        return ENFILE;
    }

    result = vfs_open(kbuf,flags,mode,&vn);
    if(result) {
        //kprintf("OPEN- vfs_open failed\n");
        kfree(kbuf);
        kfree(curthread->fdtable[index]);
        curthread->fdtable[index] = NULL;
        return result;
    }

    curthread->fdtable[index]->vn = vn;
    strcpy(curthread->fdtable[index]->file_name, kbuf);
    curthread->fdtable[index]->flags = flags;
    curthread->fdtable[index]->refcount = 1;
    curthread->fdtable[index]->offset = 0;
    curthread->fdtable[index]->lk= lock_create(kbuf);
    *retval = index;
    kfree(kbuf);
    //kprintf("Outside OPEN, Returned Index is %d, Flags- %d\n", index,curthread->fdtable[index]->flags);
    return 0;
}

int sys_close(int filehandle, int *retval) {

    //kprintf("Inside CLOSE\n");

    if(filehandle >= OPEN_MAX || filehandle < 0) {
        //kprintf("CLOSE- Bad filehandle\n");
        *retval = -1;
        return EBADF;
    }

    if(curthread->fdtable[filehandle] == NULL) {
        //kprintf("CLOSE- filetable at given index is null\n");
        *retval = -1;
        return EBADF;
    }

    if(curthread->fdtable[filehandle]->vn == NULL) {
        //kprintf("CLOSE- vnode in the given filetable index is null\n");
        *retval = -1;
        return EBADF;
    }

    if(curthread->fdtable[filehandle]->refcount == 1) {
        vfs_close(curthread->fdtable[filehandle]->vn);
        lock_destroy(curthread->fdtable[filehandle]->lk);
        kfree(curthread->fdtable[filehandle]);
        curthread->fdtable[filehandle] = NULL;
    } else {
        curthread->fdtable[filehandle]->refcount -= 1;
    }

    *retval = 0;
    return 0;
    //kprintf("Outside CLOSE\n");
}

int sys_read(int filehandle, void *buf, size_t size, int *retval) {

    if(buf == NULL) {
        return EFAULT;
    }

    if(filehandle >= OPEN_MAX || filehandle < 0) {
        //kprintf("READ- Bad Filehandle\n");
        return EBADF;
    }

    if(curthread->fdtable[filehandle] == NULL) {
        //kprintf("READ- Filetable at given index is null\n");
        return EBADF;
    }
    if (curthread->fdtable[filehandle]->flags != O_RDONLY && curthread->fdtable[filehandle]->flags != O_RDWR) {
        //kprintf("READ- Wrong Flag\n");
        return EBADF;
    }

    struct iovec iov;
    struct uio ku;
    char *kbuf = (char*)kmalloc(size);

    if(kbuf == NULL) {
        kprintf("READ- kbuf is null\n");
        return EFAULT;
    }
    lock_acquire(curthread->fdtable[filehandle]->lk);
    uio_kinit(&iov, &ku, (void*)kbuf, size ,curthread->fdtable[filehandle]->offset,UIO_READ);
    
    //kprintf("before- VOP_READ Failed\n");
    int result = VOP_READ(curthread->fdtable[filehandle]->vn, &ku);
    if(result) {
        //kprintf("READ- VOP_READ Failed\n");
        kfree(kbuf);
        lock_release(curthread->fdtable[filehandle]->lk);
        return result;
    }

            
    curthread->fdtable[filehandle]->offset = ku.uio_offset;
    copyout((const void *)kbuf, (userptr_t)buf, size);
    *retval = size - ku.uio_resid;
    //kprintf("Outside READ, Bytes read- %d\n", (size - ku.uio_resid));
    kfree(kbuf);
    lock_release(curthread->fdtable[filehandle]->lk);
    return 0;

}

int sys_write(int filehandle, const void *buf, size_t size, int *retval) {

    if(buf == NULL) {
        *retval = -1;
        return EFAULT;
    }
    //kprintf("Inside WRITE, Filehandle- %d, Size- %d\n",filehandle, size);
    if(filehandle >= OPEN_MAX || filehandle < 0) {
        //kprintf("WRITE- Bad Filehandle\n");
        *retval = -1;
        return EBADF;
    }

    if(curthread->fdtable[filehandle] == NULL) {
        //kprintf("WRITE- Filetable at given index is null\n");
        *retval = -1;
        return EBADF;
    }

    if (curthread->fdtable[filehandle]->flags == O_RDONLY) {
        //kprintf("WRITE- Wrong Flag\n");
        *retval = -1;
        return EBADF;
    }


    struct iovec iov;
    struct uio ku;
    char *kbuf = (char*)kmalloc(size);
    if(kbuf == NULL) {
        //kprintf("READ- kbuf is null\n");
        *retval = -1;
        return EINVAL;
    }

    lock_acquire(curthread->fdtable[filehandle]->lk);

    int result = copyin((const_userptr_t)buf,kbuf,size);
    if(result) {
        //kprintf("WRITE- copyin Failed\n");
        kfree(kbuf);
        lock_release(curthread->fdtable[filehandle]->lk);
        *retval = -1;
        return result;
    }

    uio_kinit(&iov, &ku, kbuf, size ,curthread->fdtable[filehandle]->offset,UIO_WRITE);

    result = VOP_WRITE(curthread->fdtable[filehandle]->vn, &ku);
    if(result) {
        //kprintf("WRITE- VOP_WRITE Failed\n");
        kfree(kbuf);
        lock_release(curthread->fdtable[filehandle]->lk);
        *retval = -1;
        return result;
    }

    curthread->fdtable[filehandle]->offset = ku.uio_offset;

    *retval = size - ku.uio_resid;

    kfree(kbuf);
    lock_release(curthread->fdtable[filehandle]->lk);
    return 0;
}

int sys_lseek(int filehandle, off_t pos, int whence, int *retval, int *retval1) {
    
    if(filehandle >= OPEN_MAX || filehandle < 0) {
        *retval = -1;
        return EBADF;
    }

    if(curthread->fdtable[filehandle] == NULL) {
        *retval = -1;
        return EBADF;
    }

    int result = 0;
    off_t offset;
    struct stat statbuf;

    lock_acquire(curthread->fdtable[filehandle]->lk);

    if(!VOP_ISSEEKABLE(curthread->fdtable[filehandle]->vn)){
        lock_release(curthread->fdtable[filehandle]->lk);
        *retval = -1;
        return ESPIPE;
    }
    switch(whence){
        case SEEK_SET:
            offset = pos;
            break;

        case SEEK_CUR:
            offset = curthread->fdtable[filehandle]->offset + pos;
            break;

        case SEEK_END:
            result = VOP_STAT(curthread->fdtable[filehandle]->vn, &statbuf);
            if(result) {
                lock_release(curthread->fdtable[filehandle]->lk);
                *retval = -1;
                return result;
            }
            offset = statbuf.st_size + pos;
            break;
        
        default:
            lock_release(curthread->fdtable[filehandle]->lk);
            *retval = -1;
            return EINVAL;
            break;
    }

    if(offset < (off_t)0) {
        *retval = -1;
        lock_release(curthread->fdtable[filehandle]->lk);
        return EINVAL;
    }
    curthread->fdtable[filehandle]->offset = offset;
    *retval = (uint32_t)((offset & 0xFFFFFFFF00000000) >> 32);
    *retval1 = (uint32_t)(offset & 0xFFFFFFFF);
    lock_release(curthread->fdtable[filehandle]->lk);
    return 0;
}
