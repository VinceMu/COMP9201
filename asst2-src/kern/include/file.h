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

struct file {
	int f_mode;
	struct vnode *f_vnode;
	struct lock *f_lock;	//lock for IO				
	off_t f_offset;
	int f_refcount;				//use for garbage collection
	
};
struct file *file_table[OPEN_MAX];



int file_open(char *file_dir, int flag, int mode, int *ret_value );
int creat_filetable(void);
//int destroy_ft(struct file_table *ft);
int put_into_table(struct file *file, int *fd);
int file_open(char *file_name, int flag, int mode, int *fd);
int sys_open(userptr_t filename, int flags, int mode, int *retval);
int sys_write(int fd, userptr_t buf, size_t size);
int search_filetable(int fd, struct file **file);
/*void u_uio_kinit(struct iovec *iov, struct uio *u, userptr_t skbuf, size_t len, off_t pos, enum uio_rw rw);
*/
#endif /* _FILE_H_ */
