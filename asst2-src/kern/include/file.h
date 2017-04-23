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



int sys_open(userptr_t filename, int flags, int mode, int *retfd);
int sys_write(int fd, userptr_t buf, size_t len, int *retval);
int sys_read(int fd, userptr_t buf, size_t size, int *retval);
int sys_close(int fd);

/*void u_uio_kinit(struct iovec *iov, struct uio *u, userptr_t skbuf, size_t len, off_t pos, enum uio_rw rw);
*/
#endif /* _FILE_H_ */
