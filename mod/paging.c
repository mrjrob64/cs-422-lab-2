#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <linux/list.h>

#include <paging.h>

#define PPAGE_MMAP_SUCCESS 0

static atomic_t allocated_pages;
static atomic_t freed_pages;
static unsigned int demand_paging = 1;
module_param(demand_paging, uint, 0644);

static DEFINE_SPINLOCK(page_list_lock);

typedef struct physical_mem_tracker
{
    atomic_t ref_counter;
    struct list_head page_list;

} physical_mem_tracker_t;

typedef struct page_list
{
    struct page * page;
    struct list_head list;

} page_list_t;





//TODO - add rm from page list
// requires a way to lock this structure since it is global

//removes the pages and frees them
void rm_pages(struct vm_area_struct * vma)
{
    
    physical_mem_tracker_t * tracker;
    page_list_t * page_list;
    page_list_t * tmp;

    tracker = (physical_mem_tracker_t *) vma->vm_private_data;

    spin_lock(&page_list_lock);

    list_for_each_entry_safe(page_list, tmp, &tracker->page_list, list)
    {
	__free_page(page_list->page); 
        list_del(&page_list->list);
	kfree(page_list);
	atomic_add(1, &freed_pages);
    }

    spin_unlock(&page_list_lock);
}

//TODO - add add to page list
//requires a way to lock this structure since it is global

void add_page(struct vm_area_struct * vma, struct page * page)
{
    page_list_t * page_list;
    physical_mem_tracker_t * tracker;

    printk(KERN_INFO "Adding page to list\n");

    page_list = (page_list_t *) kmalloc(sizeof(page_list_t), GFP_KERNEL);
    page_list->page = page;
    tracker = (physical_mem_tracker_t *) vma->vm_private_data;

    //in order to safely add must use lock
    spin_lock(&page_list_lock);
    list_add(&page_list->list, &tracker->page_list);
    spin_unlock(&page_list_lock);

    atomic_add(1, &allocated_pages);
}

static int pp(struct vm_area_struct * vma)
{
    struct page * new_page;
    unsigned pfn; 
    unsigned long start_address, end_address;
    unsigned long lower_aligned_start_address, upper_aligned_end_address;
    unsigned int numPages;
    unsigned long fault_address_aligned;
    int i;

    //find the # of pages we need to allocate
    start_address = vma->vm_start;
    end_address = vma->vm_end;

    lower_aligned_start_address = start_address & PAGE_MASK;
    upper_aligned_end_address = PAGE_ALIGN(end_address);

    //lower address rounded down and upper address rounded up
    numPages = (upper_aligned_end_address - lower_aligned_start_address) / PAGE_SIZE;

    //allocates and maps one page at a time
    for(i = 0; i < numPages; i++) {
        
        //allocates page
        new_page = alloc_page(GFP_KERNEL);
        
        //checks for if memory allocation fails
        if(new_page == NULL) {
            printk(KERN_INFO "page memory allocation failed\n");
            return VM_FAULT_OOM;
        }

        pfn = page_to_pfn(new_page);
        fault_address_aligned  = lower_aligned_start_address + (i * PAGE_SIZE);

        //checks for remapping failed / other errors
        if(remap_pfn_range(vma, fault_address_aligned, pfn, PAGE_SIZE, vma->vm_page_prot))
        {
            printk(KERN_INFO "remap_pfn_range failed\n");
            return VM_FAULT_SIGBUS;
        }

        //finds the physical memory to add
        add_page(vma, new_page);  
    }

    return VM_FAULT_NOPAGE;

}


static int do_fault(struct vm_area_struct * vma, unsigned long fault_address)
{
    struct page * new_page;
    unsigned long pfn;
    unsigned long page_aligned_address;

    printk(KERN_INFO "paging_vma_fault() invoked: took a page fault at VA 0x%lx\n", fault_address);
    new_page = alloc_page(GFP_KERNEL);
 
    //checks for if memory allocation
    if(new_page == NULL) {
        printk(KERN_INFO "page memory allocation failed\n");
        return VM_FAULT_OOM;
    }
 
    pfn = page_to_pfn(new_page);
    page_aligned_address = PAGE_MASK & fault_address;

    printk(KERN_INFO "PAGE ALIGNED fault adress: 0x%lx\n", fault_address); 
    if(remap_pfn_range(vma, page_aligned_address, pfn, PAGE_SIZE, vma->vm_page_prot))
    {
        printk(KERN_INFO "remap_pfn_range failed\n");
        return VM_FAULT_SIGBUS;
    }

    add_page(vma, new_page);

    return VM_FAULT_NOPAGE;
}
 
static vm_fault_t paging_vma_fault(struct vm_fault * vmf)
{
    struct vm_area_struct * vma = vmf->vma;
    unsigned long fault_address = (unsigned long)vmf->address;
 
    if(demand_paging) {   
        return do_fault(vma, fault_address);
    } else {
        return VM_FAULT_NOPAGE;
    }
}
 
static void paging_vma_open(struct vm_area_struct * vma)
{
    physical_mem_tracker_t * pointer;
    printk(KERN_INFO "paging_vma_open() invoked\n");
    pointer = (physical_mem_tracker_t *) vma->vm_private_data;
    atomic_add(1, &pointer->ref_counter);
 
}

static void paging_vma_close(struct vm_area_struct * vma)
{
    physical_mem_tracker_t * pointer;
    printk(KERN_INFO "paging_vma_close() invoked\n");
    pointer = (physical_mem_tracker_t *) vma->vm_private_data;
    atomic_sub(1, &pointer->ref_counter);
    if(atomic_read(&pointer->ref_counter) == 0)
    {
	
	rm_pages(vma);
        kfree(pointer);
    }
}
 
static struct vm_operations_struct paging_vma_ops =
{
    .fault = paging_vma_fault,
    .open  = paging_vma_open,
    .close = paging_vma_close
};
 
/* vma is the new virtual address segment for the process */
static int paging_mmap(struct file * filp, struct vm_area_struct * vma)
{
    int pp_ret_address;
    physical_mem_tracker_t * tracker;
    
    /* prevent Linux from mucking with our VMA (expanding it, merging it
     * with other VMAs, etc.)
     */
    vma->vm_flags |= VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_NORESERVE
              | VM_DONTDUMP | VM_PFNMAP;
 
    /* setup the vma->vm_ops, so we can catch page faults */
    vma->vm_ops = &paging_vma_ops;
   
    //initialize vma private_data struct
    vma->vm_private_data = kmalloc(sizeof(physical_mem_tracker_t), GFP_KERNEL);
    tracker = (physical_mem_tracker_t *) vma->vm_private_data;
    atomic_set(&tracker->ref_counter, 1);

    INIT_LIST_HEAD(&tracker->page_list);

    printk(KERN_INFO "paging_mmap() invoked: new VMA for pid %d from VA 0x%lx to 0x%lx\n",
        current->pid, vma->vm_start, vma->vm_end);
 
    if(!demand_paging) {
        pp_ret_address = pp(vma);
	switch(pp_ret_address)
        {
	     case VM_FAULT_OOM:
	         return -ENOMEM;
	     case VM_FAULT_NOPAGE:
		 return PPAGE_MMAP_SUCCESS;
	     case VM_FAULT_SIGBUS:
		 return -EFAULT;
	     default:
		 printk(KERN_ALERT "Unexpected Return Value pp function!\n");
		 return PPAGE_MMAP_SUCCESS;
        }
    }
    return 0;
}
 
static struct file_operations dev_ops =
{
    .mmap = paging_mmap,
};
 
static struct miscdevice dev_handle =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = PAGING_MODULE_NAME,
    .fops = &dev_ops,
};
/*** END device I/O **/
 
/*** Kernel module initialization and teardown ***/
static int kmod_paging_init(void)
{
    int status;
 
    atomic_set(&allocated_pages, 0);
    atomic_set(&freed_pages, 0);

    /* Create a character device to communicate with user-space via file I/O operations */
    status = misc_register(&dev_handle);
    if (status != 0) {
        printk(KERN_ERR "Failed to register misc. device for module\n");
        return status;
    }
 
    printk(KERN_INFO "Loaded kmod_paging module\n");
 
    return 0;
}
 
static void kmod_paging_exit(void)
{
    /* Deregister our device file */
    misc_deregister(&dev_handle);
    printk("Freed Pages %d, Allocated Pages: %d\n", atomic_read(&freed_pages), atomic_read(&allocated_pages));
    printk(KERN_INFO "Unloaded kmod_paging module\n");
}
 
module_init(kmod_paging_init);
module_exit(kmod_paging_exit);
 
/* Misc module info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Who are you?");
MODULE_DESCRIPTION("Please describe this module.");
