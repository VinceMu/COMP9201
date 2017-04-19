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
		return err
	
	file = kmalloc(sizeof(struct file));
	if(file == NULL){
		vfs_close(vn);
		return ENOMEM;
	}
	
	file->f_vnode = vn;
	file->f_offset = 0;
	file->f_refcount = 1;
	
	file->of_lock = lock_create("lock");
	if(file->of_lock == NULL){
		vfs_close(vn);
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
 
int creat_filetable(){
	
	
	curthread->t_filetable = kmalloc(sizeof(struct filetable));
	if(curthread->t_filetable == NULL)
		return ENOMEM;
	
	/*set all file to null*/
	for(int i=0; i<OPEN_MAX; i++)
		curthread->t_filetable->files[i] = NULL;
	
	
	
	for(int i=0; i<3 ;i++){
		int err = file_open("con:", RDWR, 0, &i);
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
