/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
/*
 * Put your function declarations and data types here ...
 */
 //https://www.youtube.com/watch?v=4DggLHAOhn8
struct file {
	int f_mode;
	struct vnode *f_vnode;
	struct lock *f_lock;	//lock for IO				
	off_t f_offset;
	int f_refcount;				//use for garbage collection
	
};

int file_open(char *file_dir, int flag, int mode, int *ret_value );
//int file_close();

//int f_clode();
struct file_table{
	struct file *files[OPEN_MAX];
};

int creat_filetable(void);
//int destroy_ft(struct file_table *ft);
int put_into_table(struct file *file, int *fd);
int file_open(char *file_name, int flag, int mode, int *fd);
int sys_open(userptr_t filename, int flags, int mode, int *retval);
int sys_write(int fd, userptr_t buf, size_t size, int *retval);
int search_filetable(int fd, struct file **file);


#endif /* _FILE_H_ */
