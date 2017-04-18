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
	enum {RD_ONLY, WR_ONLY, RDWR} f_accmode;
	
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


int create_ft(void);
int destroy_ft(struct file_table *ft);




#endif /* _FILE_H_ */
