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
int file_open(char *file_name, int flag, int mode, int *fd){
	struct vnode *vn;
	struct file *file;
	
	int err = vfs_open(file_name, flag, mode, &vn);
	if(err)
		return err;
	
	file = kmalloc(sizeof(struct file));
	if(file == NULL){
		vfs_close(vn);
		return ENOMEM;
	}
	
	file->f_vnode = vn;
	file->f_offset = 0;
	file->f_refcount = 1;
	
	file->f_lock = lock_create("lock");
	if(file->f_lock == NULL){
		vfs_close(vn);
		kfree(file);
		return ENOMEM;
	}
	
	err = put_into_table(file, fd);
	if(err){
		lock_destroy(file->f_lock);
		kfree(file);
		vfs_close(vn);
		return err;
	}
	
	return 0;
}

int put_into_table(struct file *file, int *fd){
	for(int i=0; i< OPEN_MAX; i++){
		if(curthread->t_filetable->files[i] == NULL){
			*fd = i;
			curthread->t_filetable->files[i] = file;
			return 0;
		}
	}
	return EMFILE;
}	
 
int creat_filetable(void){
	
	
	curthread->t_filetable = kmalloc(sizeof(struct file_table));
	if(curthread->t_filetable == NULL)
		return ENOMEM;
	
	/*set all file to null*/
	for(int i=0; i<OPEN_MAX; i++)
		curthread->t_filetable->files[i] = NULL;
	
	char dir[5];
	strcpy(dir, "con:");	
	
	for(int i=0; i<3 ;i++){
		int err = file_open(dir, O_RDWR, 0, &i);
		if(err)
			return err;		
	}
	
	return 0;
	
}
 
int sys_open(userptr_t filename, int flags, int mode, int *retval){
	char fname[PATH_MAX];
	int err = copyinstr(filename, fname, PATH_MAX, NULL);
	if(err)
		return err;
	
	return file_open(fname, flags, mode, retval);
}

int sys_write(int fd, userptr_t buf, size_t size){

	struct uio u_io;
	struct iovec u_iovec;

	struct file *file;
	int err = search_filetable(fd, &file);
	if(err)
		return err;
	
	lock_acquire(file->f_lock);

	if(file->f_mode == O_RDONLY){
		lock_release(file->f_lock);
		return EBADF;
	}

	uio_kinit(&u_iovec, &u_io, buf, size, file->f_offset, UIO_WRITE);	
    u_io.uio_space = curthread->t_addrspace;
    
    
	err = VOP_WRITE(file->f_vnode, &u_io);
	if(err){
		lock_release(file->f_lock);
		return err;
	}
	file->f_offset += size;
	lock_release(file->f_lock);

	return 0;
}

int search_filetable(int fd, struct file **file){

	if(fd<0 ||fd >= OPEN_MAX)
		return EBADF;

	*file = curthread->t_filetable->files[fd];
	if(*file == NULL)
		return EBADF;
	
	return 0;
}	
/*
void u_uio_kinit(struct iovec *iov, struct uio *u,
	  userptr_t kbuf, size_t len, off_t pos, enum uio_rw rw){
	iov->iov_kbase = kbuf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = pos;
	u->uio_resid = len;
	u->uio_segflg = UIO_SYSSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}*/
