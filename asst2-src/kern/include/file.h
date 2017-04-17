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
	enum {FD_NONE, FD_PIP, FD_INODE} f_type;
	
	struct vnode *f_vnode;
	struct lock *f_lock;	//lock for IO				
	off_t f_offset;
	int f_refcount;				//use for garbage collection
	
};

int f_open(char *file_dir, int flag, int mode, int *ret_value );
//int f_clode();
struct file_table{
	struct file *file_table[OPEN_MAX];
};


int create_ft();
int destroy_ft();
int 



#endif /* _FILE_H_ */
