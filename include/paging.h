#ifndef __PAGING_H
#define __PAGING_H

/* Seen by kernel and user */
#define PAGING_MODULE_NAME "paging"
#define DEV_NAME "/dev/" PAGING_MODULE_NAME

#include <linux/atomic.h>

typedef struct physical_mem_tracker
{
	
	atomic_t ref_counter;

} physical_mem_tracker_t;

#endif
