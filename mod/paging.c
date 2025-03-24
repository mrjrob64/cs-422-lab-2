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

#include <paging.h>


static int
do_fault(struct vm_area_struct * vma,
         unsigned long           fault_address)
{
    printk(KERN_INFO "paging_vma_fault() invoked: took a page fault at VA 0x%lx\n", fault_address);
    struct page * new_page = alloc_pages(GFP_KERNEL);
    unsigned long pfn = page_to_pfn(new_page);
    unsigned long page_aligned_address = PAGE_ALIGN(fault_address);

    if(!remap_pfn_range(vma, unsigned long vaddr, pfn, PAGE_SIZE, vma->vm_page_root))
    {
        //do something we wanna make sure the page isn't freed if it isn't allocated maybe
    }
    return VM_FAULT_SIGBUS;
}

static vm_fault_t
paging_vma_fault(struct vm_fault * vmf)
{
    struct vm_area_struct * vma = vmf->vma;
    unsigned long fault_address = (unsigned long)vmf->address;

    return do_fault(vma, fault_address);
}

static void
paging_vma_open(struct vm_area_struct * vma)
{
    printk(KERN_INFO "paging_vma_open() invoked\n");
    physical_mem_tracker_t * pointer = (physical_mem_tracker_t *) vma->private_data;
    atomic_add(&pointer->ref_counter, 1);

}

static void
paging_vma_close(struct vm_area_struct * vma)
{
    printk(KERN_INFO "paging_vma_close() invoked\n");
    physical_mem_tracker_t * pointer = (physical_mem_tracker_t *) vma->private_data;
    atomic_dec(&pointer->ref_counter, 1);
    if(atomic_read(&pointer->ref_counter) == 0)
    {
        kfree(pointer);
    }
}

static struct vm_operations_struct
paging_vma_ops = 
{
    .fault = paging_vma_fault,
    .open  = paging_vma_open,
    .close = paging_vma_close
};

/* vma is the new virtual address segment for the process */
static int
paging_mmap(struct file           * filp,
            struct vm_area_struct * vma)
{
    /* prevent Linux from mucking with our VMA (expanding it, merging it 
     * with other VMAs, etc.)
     */
    vma->vm_flags |= VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_NORESERVE
              | VM_DONTDUMP | VM_PFNMAP;

    /* setup the vma->vm_ops, so we can catch page faults */
    vma->vm_ops = &paging_vma_ops;
    
    //initialize vma private_data struct
    vma->private_data = (physical_mem_tracker_t *) kmalloc(sizeof(physical_mem_tracker_t), GFP_KERNEL);
    atomic_set(&vma->private_data->ref_counter, 1);

    printk(KERN_INFO "paging_mmap() invoked: new VMA for pid %d from VA 0x%lx to 0x%lx\n",
        current->pid, vma->vm_start, vma->vm_end);

    return 0;
}

static struct file_operations
dev_ops =
{
    .mmap = paging_mmap,
};

static struct miscdevice
dev_handle =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = PAGING_MODULE_NAME,
    .fops = &dev_ops,
};
/*** END device I/O **/

/*** Kernel module initialization and teardown ***/
static int
kmod_paging_init(void)
{
    int status;

    /* Create a character device to communicate with user-space via file I/O operations */
    status = misc_register(&dev_handle);
    if (status != 0) {
        printk(KERN_ERR "Failed to register misc. device for module\n");
        return status;
    }

    printk(KERN_INFO "Loaded kmod_paging module\n");

    return 0;
}

static void
kmod_paging_exit(void)
{
    /* Deregister our device file */
    misc_deregister(&dev_handle);

    printk(KERN_INFO "Unloaded kmod_paging module\n");
}

module_init(kmod_paging_init);
module_exit(kmod_paging_exit);

/* Misc module info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Who are you?");
MODULE_DESCRIPTION("Please describe this module.");
