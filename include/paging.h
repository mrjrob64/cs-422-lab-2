#ifndef __PAGING_H__
#define __PAGING_H__

#define PAGING_MODULE_NAME "paging"
#define DEV_NAME "/dev/" PAGING_MODULE_NAME

#ifdef __KERNEL__
    #include <linux/types.h>
    #include <linux/atomic.h>
#else
    #include <stdatomic.h> // Use C11 atomics in user-space
#endif

typedef struct physical_mem_tracker {
#ifdef __KERNEL__
    atomic_t ref_counter;
#else
    atomic_int ref_counter;
#endif
} physical_mem_tracker_t;

#endif
