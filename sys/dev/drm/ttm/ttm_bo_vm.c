/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 */
/*
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * <kib@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 *$FreeBSD: head/sys/dev/drm2/ttm/ttm_bo_vm.c 253710 2013-07-27 16:44:37Z kib $
 */

#include "opt_vm.h"

#define pr_fmt(fmt) "[TTM] " fmt

#include <drm/ttm/ttm_module.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <linux/errno.h>
#include <linux/export.h>

#include <vm/vm_page2.h>

RB_GENERATE(ttm_bo_device_buffer_objects, ttm_buffer_object, vm_rb,
    ttm_bo_cmp_rb_tree_items);


#define TTM_BO_VM_NUM_PREFAULT 16

int
ttm_bo_cmp_rb_tree_items(struct ttm_buffer_object *a,
    struct ttm_buffer_object *b)
{
    if (a->vm_node->start < b->vm_node->start) {
        return (-1);
    } else if (a->vm_node->start > b->vm_node->start) {
        return (1);
    } else {
        return (0);
    }
}


static struct ttm_buffer_object *ttm_bo_vm_lookup_rb(struct ttm_bo_device *bdev,
						     unsigned long page_start,
						     unsigned long num_pages)
{
	unsigned long cur_offset;
	struct ttm_buffer_object *bo;
	struct ttm_buffer_object *best_bo = NULL;

	bo = RB_ROOT(&bdev->addr_space_rb);
	while (bo != NULL) {
		cur_offset = bo->vm_node->start;
		if (page_start >= cur_offset) {
			best_bo = bo;
			if (page_start == cur_offset)
				break;
			bo = RB_RIGHT(bo, vm_rb);
		} else
			bo = RB_LEFT(bo, vm_rb);
	}

	if (unlikely(best_bo == NULL))
		return NULL;

	if (unlikely((best_bo->vm_node->start + best_bo->num_pages) <
		     (page_start + num_pages)))
		return NULL;

	return best_bo;
}

static int
ttm_bo_vm_fault(vm_object_t vm_obj, vm_ooffset_t offset,
    int prot, vm_page_t *mres)
{
	struct ttm_buffer_object *bo = vm_obj->handle;
	struct ttm_bo_device *bdev = bo->bdev;
	struct ttm_tt *ttm = NULL;
	vm_page_t m, m1, oldm;
	int ret;
	int retval = VM_PAGER_OK;
	struct ttm_mem_type_manager *man =
		&bdev->man[bo->mem.mem_type];

	vm_object_pip_add(vm_obj, 1);
	oldm = *mres;
	if (oldm != NULL) {
		vm_page_remove(oldm);
		*mres = NULL;
	} else
		oldm = NULL;
retry:
	VM_OBJECT_UNLOCK(vm_obj);
	m = NULL;

reserve:
	ret = ttm_bo_reserve(bo, false, false, false, 0);
	if (unlikely(ret != 0)) {
		if (ret == -EBUSY) {
			lwkt_yield();
			goto reserve;
		}
	}

	if (bdev->driver->fault_reserve_notify) {
		ret = bdev->driver->fault_reserve_notify(bo);
		switch (ret) {
		case 0:
			break;
		case -EBUSY:
		case -ERESTARTSYS:
		case -EINTR:
			lwkt_yield();
			goto reserve;
		default:
			retval = VM_PAGER_ERROR;
			goto out_unlock;
		}
	}

	/*
	 * Wait for buffer data in transit, due to a pipelined
	 * move.
	 */

	lockmgr(&bdev->fence_lock, LK_EXCLUSIVE);
	if (test_bit(TTM_BO_PRIV_FLAG_MOVING, &bo->priv_flags)) {
		/*
		 * Here, the behavior differs between Linux and FreeBSD.
		 *
		 * On Linux, the wait is interruptible (3rd argument to
		 * ttm_bo_wait). There must be some mechanism to resume
		 * page fault handling, once the signal is processed.
		 *
		 * On FreeBSD, the wait is uninteruptible. This is not a
		 * problem as we can't end up with an unkillable process
		 * here, because the wait will eventually time out.
		 *
		 * An example of this situation is the Xorg process
		 * which uses SIGALRM internally. The signal could
		 * interrupt the wait, causing the page fault to fail
		 * and the process to receive SIGSEGV.
		 */
		ret = ttm_bo_wait(bo, false, false, false);
		lockmgr(&bdev->fence_lock, LK_RELEASE);
		if (unlikely(ret != 0)) {
			retval = VM_PAGER_ERROR;
			goto out_unlock;
		}
	} else
		lockmgr(&bdev->fence_lock, LK_RELEASE);

	ret = ttm_mem_io_lock(man, true);
	if (unlikely(ret != 0)) {
		retval = VM_PAGER_ERROR;
		goto out_unlock;
	}
	ret = ttm_mem_io_reserve_vm(bo);
	if (unlikely(ret != 0)) {
		retval = VM_PAGER_ERROR;
		goto out_io_unlock;
	}

	/*
	 * Strictly, we're not allowed to modify vma->vm_page_prot here,
	 * since the mmap_sem is only held in read mode. However, we
	 * modify only the caching bits of vma->vm_page_prot and
	 * consider those bits protected by
	 * the bo->mutex, as we should be the only writers.
	 * There shouldn't really be any readers of these bits except
	 * within vm_insert_mixed()? fork?
	 *
	 * TODO: Add a list of vmas to the bo, and change the
	 * vma->vm_page_prot when the object changes caching policy, with
	 * the correct locks held.
	 */
	if (!bo->mem.bus.is_iomem) {
		/* Allocate all page at once, most common usage */
		ttm = bo->ttm;
		if (ttm->bdev->driver->ttm_tt_populate(ttm)) {
			retval = VM_PAGER_ERROR;
			goto out_io_unlock;
		}
	}

	if (bo->mem.bus.is_iomem) {
		m = vm_phys_fictitious_to_vm_page(bo->mem.bus.base +
		    bo->mem.bus.offset + offset);
		pmap_page_set_memattr(m, ttm_io_prot(bo->mem.placement));
	} else {
		ttm = bo->ttm;
		m = (struct vm_page *)ttm->pages[OFF_TO_IDX(offset)];
		if (unlikely(!m)) {
			retval = VM_PAGER_ERROR;
			goto out_io_unlock;
		}
		pmap_page_set_memattr(m,
		    (bo->mem.placement & TTM_PL_FLAG_CACHED) ?
		    VM_MEMATTR_WRITE_BACK : ttm_io_prot(bo->mem.placement));
	}

	VM_OBJECT_LOCK(vm_obj);
	if ((m->busy_count & PBUSY_LOCKED) != 0) {
#if 0
		vm_page_sleep(m, "ttmpbs");
#endif
		ttm_mem_io_unlock(man);
		ttm_bo_unreserve(bo);
		goto retry;
	}
	m->valid = VM_PAGE_BITS_ALL;
	*mres = m;
	m1 = vm_page_lookup(vm_obj, OFF_TO_IDX(offset));
	if (m1 == NULL) {
		vm_page_insert(m, vm_obj, OFF_TO_IDX(offset));
	} else {
		KASSERT(m == m1,
		    ("inconsistent insert bo %p m %p m1 %p offset %jx",
		    bo, m, m1, (uintmax_t)offset));
	}
	vm_page_busy_try(m, FALSE);

	if (oldm != NULL) {
		vm_page_free(oldm);
	}

out_io_unlock1:
	ttm_mem_io_unlock(man);
out_unlock1:
	ttm_bo_unreserve(bo);
	vm_object_pip_wakeup(vm_obj);
	return (retval);

out_io_unlock:
	VM_OBJECT_LOCK(vm_obj);
	goto out_io_unlock1;

out_unlock:
	VM_OBJECT_LOCK(vm_obj);
	goto out_unlock1;
}

static int
ttm_bo_vm_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	/*
	 * On Linux, a reference to the buffer object is acquired here.
	 * The reason is that this function is not called when the
	 * mmap() is initialized, but only when a process forks for
	 * instance. Therefore on Linux, the reference on the bo is
	 * acquired either in ttm_bo_mmap() or ttm_bo_vm_open(). It's
	 * then released in ttm_bo_vm_close().
	 *
	 * Here, this function is called during mmap() intialization.
	 * Thus, the reference acquired in ttm_bo_mmap_single() is
	 * sufficient.
	 */
	*color = 0;
	return (0);
}

static void
ttm_bo_vm_dtor(void *handle)
{
	struct ttm_buffer_object *bo = handle;

	ttm_bo_unref(&bo);
}

static struct cdev_pager_ops ttm_pager_ops = {
	.cdev_pg_fault = ttm_bo_vm_fault,
	.cdev_pg_ctor = ttm_bo_vm_ctor,
	.cdev_pg_dtor = ttm_bo_vm_dtor
};

int
ttm_bo_mmap_single(struct ttm_bo_device *bdev, vm_ooffset_t *offset, vm_size_t size,
    struct vm_object **obj_res, int nprot)
{
	struct ttm_bo_driver *driver;
	struct ttm_buffer_object *bo;
	struct vm_object *vm_obj;
	int ret;

	*obj_res = NULL;

	lockmgr(&bdev->vm_lock, LK_EXCLUSIVE);
	bo = ttm_bo_vm_lookup_rb(bdev, OFF_TO_IDX(*offset), OFF_TO_IDX(size));
	if (likely(bo != NULL))
		kref_get(&bo->kref);
	lockmgr(&bdev->vm_lock, LK_RELEASE);

	if (unlikely(bo == NULL)) {
		pr_err("Could not find buffer object to map\n");
		return (EINVAL);
	}

	driver = bo->bdev->driver;
	if (unlikely(!driver->verify_access)) {
		ret = EPERM;
		goto out_unref;
	}
	ret = -driver->verify_access(bo);
	if (unlikely(ret != 0))
		goto out_unref;

	vm_obj = cdev_pager_allocate(bo, OBJT_MGTDEVICE, &ttm_pager_ops,
	    size, nprot, 0, curthread->td_ucred);

	if (vm_obj == NULL) {
		ret = EINVAL;
		goto out_unref;
	}
	/*
	 * Note: We're transferring the bo reference to vm_obj->handle here.
	 */
	*offset = 0;
	*obj_res = vm_obj;
	return 0;
out_unref:
	ttm_bo_unref(&bo);
	return ret;
}
EXPORT_SYMBOL(ttm_bo_mmap);

void
ttm_bo_release_mmap(struct ttm_buffer_object *bo)
{
	vm_object_t vm_obj;
	vm_page_t m;
	int i;

	vm_obj = cdev_pager_lookup(bo);
	if (vm_obj == NULL)
		return;

	VM_OBJECT_LOCK(vm_obj);
	for (i = 0; i < bo->num_pages; i++) {
		m = vm_page_lookup_busy_wait(vm_obj, i, TRUE, "ttm_unm");
		if (m == NULL)
			continue;
		cdev_pager_free_page(vm_obj, m);
	}
	VM_OBJECT_UNLOCK(vm_obj);

	vm_object_deallocate(vm_obj);
}

#if 0
int ttm_fbdev_mmap(struct vm_area_struct *vma, struct ttm_buffer_object *bo)
{
	if (vma->vm_pgoff != 0)
		return -EACCES;

	vma->vm_ops = &ttm_bo_vm_ops;
	vma->vm_private_data = ttm_bo_reference(bo);
	vma->vm_flags |= VM_IO | VM_MIXEDMAP | VM_DONTEXPAND;
	return 0;
}
EXPORT_SYMBOL(ttm_fbdev_mmap);
#endif
