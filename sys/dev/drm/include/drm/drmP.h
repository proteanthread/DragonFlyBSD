/*
 * Internal Header for the Direct Rendering Manager
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * Copyright (c) 2009-2010, Code Aurora Forum.
 * All rights reserved.
 *
 * Author: Rickard E. (Rik) Faith <faith@valinux.com>
 * Author: Gareth Hughes <gareth@valinux.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

#include <linux/agp_backend.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include <asm/pgalloc.h>
#include <asm/uaccess.h>

#include <uapi_drm/drm.h>
#include <uapi_drm/drm_mode.h>

#include <drm/drm_agpsupport.h>
#include <drm/drm_crtc.h>
#include <drm/drm_global.h>
#include <drm/drm_hashtab.h>
#include <drm/drm_mem_util.h>
#include <drm/drm_mm.h>
#include <drm/drm_os_linux.h>
#include <uapi_drm/drm_sarea.h>
#include <drm/drm_vma_manager.h>

#include <sys/device.h>

#include <vm/vm_extern.h>
#include <vm/vm_pager.h>

struct module;

struct drm_file;
struct drm_device;
struct drm_agp_head;
struct drm_local_map;
struct drm_device_dma;
struct drm_dma_handle;
struct drm_gem_object;

struct device_node;
struct videomode;
struct reservation_object;
struct dma_buf_attachment;

/*
 * The following categories are defined:
 *
 * CORE: Used in the generic drm code: drm_ioctl.c, drm_mm.c, drm_memory.c, ...
 *	 This is the category used by the DRM_DEBUG() macro.
 *
 * DRIVER: Used in the vendor specific part of the driver: i915, radeon, ...
 *	   This is the category used by the DRM_DEBUG_DRIVER() macro.
 *
 * KMS: used in the modesetting code.
 *	This is the category used by the DRM_DEBUG_KMS() macro.
 *
 * PRIME: used in the prime code.
 *	  This is the category used by the DRM_DEBUG_PRIME() macro.
 *
 * ATOMIC: used in the atomic code.
 *	  This is the category used by the DRM_DEBUG_ATOMIC() macro.
 *
 * PID: used as modifier to include PID number in messages.
 *	  This is the category used by the all debug macros.
 *
 * FIOCTL: used in failed ioctl debugging.
 *	  This is the category used by the DRM_DEBUG_FIOCTL() macro.
 *
 * IOCTL: used in ioctl debugging.
 *	  This is the category used by the DRM_DEBUG_IOCTL() macro.
 *
 * VBLANK: used in vblank debugging.
 *	  This is the category used by the DRM_DEBUG_VBLANK() macro.
 *
 * VBL: used for verbose debug message in the vblank code
 *	  This is the category used by the DRM_DEBUG_VBL() macro.
 *
 * Enabling verbose debug messages is done through the drm.debug parameter,
 * each category being enabled by a bit.
 *
 * drm.debug=0x1 will enable CORE messages
 * drm.debug=0x2 will enable DRIVER messages
 * drm.debug=0x3 will enable CORE and DRIVER messages
 * ...
 * drm.debug=0x3f will enable all messages
 *
 * An interesting feature is that it's possible to enable verbose logging at
 * run-time by using hw.drm.debug sysctl variable:
 *   # sysctl hw.drm.debug=0xfff
 */
#define DRM_UT_CORE 		0x01
#define DRM_UT_DRIVER		0x02
#define DRM_UT_KMS		0x04
#define DRM_UT_PRIME		0x08
#define DRM_UT_ATOMIC		0x10
#define DRM_UT_VBL		0x20
/* Extra DragonFly debug categories */
#ifdef __DragonFly__
#define DRM_UT_RES7		0x40	/* reserved for future expansion */
#define DRM_UT_RES8		0x80	/* reserved for future expansion */
#define DRM_UT_PID		0x100
#define DRM_UT_FIOCTL		0x200
#define DRM_UT_IOCTL		0x400
#define DRM_UT_VBLANK		0x800
#endif

extern __printflike(2, 3)
void drm_ut_debug_printk(const char *function_name,
			 const char *format, ...);
extern __printflike(2, 3)
void drm_err(const char *func, const char *format, ...);

/***********************************************************************/
/** \name DRM template customization defaults */
/*@{*/

/* driver capabilities and requirements mask */
#define DRIVER_USE_AGP			0x1
#define DRIVER_PCI_DMA			0x8
#define DRIVER_SG			0x10
#define DRIVER_HAVE_DMA			0x20
#define DRIVER_HAVE_IRQ			0x40
#define DRIVER_IRQ_SHARED		0x80
#define DRIVER_GEM			0x1000
#define DRIVER_MODESET			0x2000
#define DRIVER_PRIME			0x4000
#define DRIVER_RENDER			0x8000
#define DRIVER_ATOMIC			0x10000
#define DRIVER_KMS_LEGACY_CONTEXT	0x20000

/***********************************************************************/
/** \name Macros to make printk easier */
/*@{*/

/**
 * Error output.
 *
 * \param fmt printf() like format string.
 * \param arg arguments
 */
#define DRM_ERROR(fmt, ...)				\
	drm_err(__func__, fmt, ##__VA_ARGS__)

/**
 * Rate limited error output.  Like DRM_ERROR() but won't flood the log.
 *
 * \param fmt printf() like format string.
 * \param arg arguments
 */
#define DRM_ERROR_RATELIMITED(fmt, ...)				\
({									\
	static struct krate krate_derr = { .freq = 1, .count = -16 };	\
									\
	krateprintf(&krate_derr, "error: [" DRM_NAME ":%s] *ERROR* "	\
	    fmt, __func__, ##__VA_ARGS__);				\
})

#define DRM_INFO(fmt, ...)				\
	printk(KERN_INFO "[" DRM_NAME "] " fmt, ##__VA_ARGS__)

#define DRM_INFO_ONCE(fmt, ...)				\
	printk_once(KERN_INFO "[" DRM_NAME "] " fmt, ##__VA_ARGS__)

/**
 * Debug output.
 *
 * \param fmt printf() like format string.
 * \param arg arguments
 */
#define DRM_DEBUG(fmt, args...)						\
	do {								\
		if (unlikely(drm_debug & DRM_UT_CORE))			\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)

#define DRM_DEBUG_DRIVER(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_DRIVER))		\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_KMS(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_KMS))			\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_PRIME(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_PRIME))			\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_ATOMIC(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_ATOMIC))		\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_VBL(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_VBL))			\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#ifdef __DragonFly__
#define DRM_DEBUG_FIOCTL(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_FIOCTL))		\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_IOCTL(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_IOCTL))			\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define DRM_DEBUG_VBLANK(fmt, args...)					\
	do {								\
		if (unlikely(drm_debug & DRM_UT_VBLANK))		\
			drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#endif

/*@}*/

/***********************************************************************/
/** \name Internal types and structures */
/*@{*/

#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	UID_ROOT
#define DRM_DEV_GID	GID_WHEEL

#define DRM_CURPROC		curthread
#define DRM_STRUCTPROC		struct thread
#define DRM_LOCK(dev)		lockmgr(&(dev)->struct_mutex, LK_EXCLUSIVE)
#define DRM_UNLOCK(dev)		lockmgr(&(dev)->struct_mutex, LK_RELEASE)

#if defined(INVARIANTS)
#define	DRM_LOCK_ASSERT(dev)	KKASSERT(lockstatus(&(dev)->struct_mutex, curthread) != 0);
#define	DRM_UNLOCK_ASSERT(dev)	KKASSERT(lockstatus(&(dev)->struct_mutex, curthread) == 0);
#else
#define	DRM_LOCK_ASSERT(d)
#define	DRM_UNLOCK_ASSERT(d)
#endif

#define DRM_SYSCTL_HANDLER_ARGS	(SYSCTL_HANDLER_ARGS)

#define drm_get_device_from_kdev(_kdev) (_kdev->si_drv1)

int vm_phys_fictitious_reg_range(vm_paddr_t start, vm_paddr_t end,
    vm_memattr_t memattr);
void vm_phys_fictitious_unreg_range(vm_paddr_t start, vm_paddr_t end);
vm_page_t vm_phys_fictitious_to_vm_page(vm_paddr_t pa);

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

/**
 * Ioctl function type.
 *
 * \param inode device inode.
 * \param file_priv DRM file private pointer.
 * \param cmd command.
 * \param arg argument.
 */
typedef int drm_ioctl_t(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

typedef int drm_ioctl_compat_t(struct file *filp, unsigned int cmd,
			       unsigned long arg);

#define DRM_IOCTL_NR(n)                ((n) & 0xff)

#define DRM_AUTH	0x1
#define DRM_MASTER	0x2
#define DRM_ROOT_ONLY	0x4
#define DRM_CONTROL_ALLOW 0x8
#define DRM_UNLOCKED	0x10
#define DRM_RENDER_ALLOW 0x20

struct drm_ioctl_desc {
	unsigned int cmd;
	int flags;
	drm_ioctl_t *func;
	unsigned int cmd_drv;
	const char *name;
};

/**
 * Creates a driver or general drm_ioctl_desc array entry for the given
 * ioctl, for use by drm_ioctl().
 */

#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)				\
	[DRM_IOCTL_NR(DRM_##ioctl)] = {					\
		.cmd = DRM_##ioctl,					\
		.func = _func,						\
		.flags = _flags,					\
		.cmd_drv = DRM_IOCTL_##ioctl,				\
		.name = #ioctl						\
	 }

/* Event queued up for userspace to read */
struct drm_pending_event {
	struct drm_event *event;
	struct list_head link;
	struct list_head pending_link;
	struct drm_file *file_priv;
	pid_t pid; /* pid of requester, no guarantee it's valid by the time
		      we deliver the event, for tracing only */
	void (*destroy)(struct drm_pending_event *event);
};

/* initial implementaton using a linked list - todo hashtab */
struct drm_prime_file_private {
	struct list_head head;
	struct lock lock;
};

/** File private data */
struct drm_file {
	unsigned authenticated :1;
	/* Whether we're master for a minor. Protected by master_mutex */
	unsigned is_master :1;
	/* true when the client has asked us to expose stereo 3D mode flags */
	unsigned stereo_allowed :1;
	/*
	 * true if client understands CRTC primary planes and cursor planes
	 * in the plane list
	 */
	unsigned universal_planes:1;
	/* true if client understands atomic properties */
	unsigned atomic:1;
	/*
	 * This client is allowed to gain master privileges for @master.
	 * Protected by struct drm_device::master_mutex.
	 */
	unsigned allowed_master:1;

	pid_t		  pid;
	uid_t		  uid;
	drm_magic_t magic;
	struct list_head lhead;
	struct drm_minor *minor;
	unsigned long lock_count;

	/** Mapping of mm object handles to object pointers. */
	struct idr object_idr;
	/** Lock for synchronization of access to object_idr. */
	struct lock table_lock;

	struct file *filp;
	void *driver_priv;

	struct drm_master *masterp;
	/**
	 * fbs - List of framebuffers associated with this file.
	 *
	 * Protected by fbs_lock. Note that the fbs list holds a reference on
	 * the fb object to prevent it from untimely disappearing.
	 */
	struct list_head fbs;
	struct lock fbs_lock;

	/** User-created blob properties; this retains a reference on the
	 *  property. */
	struct list_head blobs;

	wait_queue_head_t event_wait;
	struct list_head pending_event_list;
	struct list_head event_list;
	int event_space;

	struct lock event_read_lock;

	struct drm_prime_file_private prime;

#ifdef __DragonFly__
	struct drm_device *dev;
	int		  master;
	unsigned long ioctl_count;
	struct kqinfo	  dkq;
#endif
};

/**
 * Lock data.
 */
struct drm_lock_data {
	struct drm_hw_lock *hw_lock;	/**< Hardware lock */
	/** Private of lock holder's file (NULL=kernel) */
	struct drm_file *file_priv;
	wait_queue_head_t lock_queue;	/**< Queue of blocked processes */
	unsigned long lock_time;	/**< Time of last lock in jiffies */
	struct spinlock spinlock;
	uint32_t kernel_waiters;
	uint32_t user_waiters;
	int idle_has_lock;
};

/**
 * GEM specific mm private for tracking GEM objects
 */
struct drm_gem_mm {
	struct drm_vma_offset_manager vma_manager;
	struct drm_mm offset_manager;	/**< Offset mgmt for buffer objects */
	struct drm_open_hash offset_hash; /**< User token hash table for maps */
	struct unrhdr *idxunr;
};

/**
 * struct drm_master - drm master structure
 *
 * @refcount: Refcount for this master object.
 * @minor: Link back to minor char device we are master for. Immutable.
 * @unique: Unique identifier: e.g. busid. Protected by drm_global_mutex.
 * @unique_len: Length of unique field. Protected by drm_global_mutex.
 * @magic_map: Map of used authentication tokens. Protected by struct_mutex.
 * @lock: DRI lock information.
 * @driver_priv: Pointer to driver-private information.
 */
struct drm_master {
	struct kref refcount;
	struct drm_minor *minor;
	char *unique;
	int unique_len;
	struct idr magic_map;
	struct drm_lock_data lock;
	void *driver_priv;
};

/* Size of ringbuffer for vblank timestamps. Just double-buffer
 * in initial implementation.
 */
#define DRM_VBLANKTIME_RBSIZE 2

/* Flags and return codes for get_vblank_timestamp() driver function. */
#define DRM_CALLED_FROM_VBLIRQ 1
#define DRM_VBLANKTIME_SCANOUTPOS_METHOD (1 << 0)
#define DRM_VBLANKTIME_IN_VBLANK         (1 << 1)

/* get_scanout_position() return flags */
#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)

/**
 * DRM driver structure. This structure represent the common code for
 * a family of cards. There will one drm_device for each card present
 * in this family
 */
struct drm_driver {
	int (*load) (struct drm_device *, unsigned long flags);
	int (*firstopen) (struct drm_device *);
	int (*open) (struct drm_device *, struct drm_file *);
	void (*preclose) (struct drm_device *, struct drm_file *file_priv);
	void (*postclose) (struct drm_device *, struct drm_file *);
	void (*lastclose) (struct drm_device *);
	int (*unload) (struct drm_device *);
	int (*suspend) (struct drm_device *, pm_message_t state);
	int (*resume) (struct drm_device *);
	int (*dma_ioctl) (struct drm_device *dev, void *data, struct drm_file *file_priv);
	int (*dma_quiescent) (struct drm_device *);
	int (*context_dtor) (struct drm_device *dev, int context);
	int (*set_busid)(struct drm_device *dev, struct drm_master *master);

	/**
	 * get_vblank_counter - get raw hardware vblank counter
	 * @dev: DRM device
	 * @pipe: counter to fetch
	 *
	 * Driver callback for fetching a raw hardware vblank counter for @crtc.
	 * If a device doesn't have a hardware counter, the driver can simply
	 * return the value of drm_vblank_count. The DRM core will account for
	 * missed vblank events while interrupts where disabled based on system
	 * timestamps.
	 *
	 * Wraparound handling and loss of events due to modesetting is dealt
	 * with in the DRM core code.
	 *
	 * RETURNS
	 * Raw vblank counter value.
	 */
	u32 (*get_vblank_counter) (struct drm_device *dev, unsigned int pipe);

	/**
	 * enable_vblank - enable vblank interrupt events
	 * @dev: DRM device
	 * @pipe: which irq to enable
	 *
	 * Enable vblank interrupts for @crtc.  If the device doesn't have
	 * a hardware vblank counter, this routine should be a no-op, since
	 * interrupts will have to stay on to keep the count accurate.
	 *
	 * RETURNS
	 * Zero on success, appropriate errno if the given @crtc's vblank
	 * interrupt cannot be enabled.
	 */
	int (*enable_vblank) (struct drm_device *dev, unsigned int pipe);

	/**
	 * disable_vblank - disable vblank interrupt events
	 * @dev: DRM device
	 * @pipe: which irq to enable
	 *
	 * Disable vblank interrupts for @crtc.  If the device doesn't have
	 * a hardware vblank counter, this routine should be a no-op, since
	 * interrupts will have to stay on to keep the count accurate.
	 */
	void (*disable_vblank) (struct drm_device *dev, unsigned int pipe);

	/**
	 * Called by \c drm_device_is_agp.  Typically used to determine if a
	 * card is really attached to AGP or not.
	 *
	 * \param dev  DRM device handle
	 *
	 * \returns
	 * One of three values is returned depending on whether or not the
	 * card is absolutely \b not AGP (return of 0), absolutely \b is AGP
	 * (return of 1), or may or may not be AGP (return of 2).
	 */
	int (*device_is_agp) (struct drm_device *dev);

	/**
	 * Called by vblank timestamping code.
	 *
	 * Return the current display scanout position from a crtc, and an
	 * optional accurate ktime_get timestamp of when position was measured.
	 *
	 * \param dev  DRM device.
	 * \param pipe Id of the crtc to query.
	 * \param flags Flags from the caller (DRM_CALLED_FROM_VBLIRQ or 0).
	 * \param *vpos Target location for current vertical scanout position.
	 * \param *hpos Target location for current horizontal scanout position.
	 * \param *stime Target location for timestamp taken immediately before
	 *               scanout position query. Can be NULL to skip timestamp.
	 * \param *etime Target location for timestamp taken immediately after
	 *               scanout position query. Can be NULL to skip timestamp.
	 * \param mode Current display timings.
	 *
	 * Returns vpos as a positive number while in active scanout area.
	 * Returns vpos as a negative number inside vblank, counting the number
	 * of scanlines to go until end of vblank, e.g., -1 means "one scanline
	 * until start of active scanout / end of vblank."
	 *
	 * \return Flags, or'ed together as follows:
	 *
	 * DRM_SCANOUTPOS_VALID = Query successful.
	 * DRM_SCANOUTPOS_INVBL = Inside vblank.
	 * DRM_SCANOUTPOS_ACCURATE = Returned position is accurate. A lack of
	 * this flag means that returned position may be offset by a constant
	 * but unknown small number of scanlines wrt. real scanout position.
	 *
	 */
	int (*get_scanout_position) (struct drm_device *dev, unsigned int pipe,
				     unsigned int flags, int *vpos, int *hpos,
				     ktime_t *stime, ktime_t *etime,
				     const struct drm_display_mode *mode);

	/**
	 * Called by \c drm_get_last_vbltimestamp. Should return a precise
	 * timestamp when the most recent VBLANK interval ended or will end.
	 *
	 * Specifically, the timestamp in @vblank_time should correspond as
	 * closely as possible to the time when the first video scanline of
	 * the video frame after the end of VBLANK will start scanning out,
	 * the time immediately after end of the VBLANK interval. If the
	 * @crtc is currently inside VBLANK, this will be a time in the future.
	 * If the @crtc is currently scanning out a frame, this will be the
	 * past start time of the current scanout. This is meant to adhere
	 * to the OpenML OML_sync_control extension specification.
	 *
	 * \param dev dev DRM device handle.
	 * \param pipe crtc for which timestamp should be returned.
	 * \param *max_error Maximum allowable timestamp error in nanoseconds.
	 *                   Implementation should strive to provide timestamp
	 *                   with an error of at most *max_error nanoseconds.
	 *                   Returns true upper bound on error for timestamp.
	 * \param *vblank_time Target location for returned vblank timestamp.
	 * \param flags 0 = Defaults, no special treatment needed.
	 * \param       DRM_CALLED_FROM_VBLIRQ = Function is called from vblank
	 *	        irq handler. Some drivers need to apply some workarounds
	 *              for gpu-specific vblank irq quirks if flag is set.
	 *
	 * \returns
	 * Zero if timestamping isn't supported in current display mode or a
	 * negative number on failure. A positive status code on success,
	 * which describes how the vblank_time timestamp was computed.
	 */
	int (*get_vblank_timestamp) (struct drm_device *dev, unsigned int pipe,
				     int *max_error,
				     struct timeval *vblank_time,
				     unsigned flags);

	/* these have to be filled in */

	irqreturn_t(*irq_handler) (int irq, void *arg);
	void (*irq_preinstall) (struct drm_device *dev);
	int (*irq_postinstall) (struct drm_device *dev);
	void (*irq_uninstall) (struct drm_device *dev);

	/* Master routines */
	int (*master_create)(struct drm_device *dev, struct drm_master *master);
	void (*master_destroy)(struct drm_device *dev, struct drm_master *master);
	/**
	 * master_set is called whenever the minor master is set.
	 * master_drop is called whenever the minor master is dropped.
	 */

	int (*master_set)(struct drm_device *dev, struct drm_file *file_priv,
			  bool from_open);
	void (*master_drop)(struct drm_device *dev, struct drm_file *file_priv,
			    bool from_release);

	int (*debugfs_init)(struct drm_minor *minor);
	void (*debugfs_cleanup)(struct drm_minor *minor);

	/**
	 * @gem_free_object: deconstructor for drm_gem_objects
	 *
	 * This is deprecated and should not be used by new drivers. Use
	 * @gem_free_object_unlocked instead.
	 */
	void (*gem_free_object) (struct drm_gem_object *obj);

	/**
	 * @gem_free_object_unlocked: deconstructor for drm_gem_objects
	 *
	 * This is for drivers which are not encumbered with dev->struct_mutex
	 * legacy locking schemes. Use this hook instead of @gem_free_object.
	 */
	void (*gem_free_object_unlocked) (struct drm_gem_object *obj);

	int (*gem_open_object) (struct drm_gem_object *, struct drm_file *);
	void (*gem_close_object) (struct drm_gem_object *, struct drm_file *);

	/**
	 * Hook for allocating the GEM object struct, for use by core
	 * helpers.
	 */
	struct drm_gem_object *(*gem_create_object)(struct drm_device *dev,
						    size_t size);

	/* prime: */
	/* export handle -> fd (see drm_gem_prime_handle_to_fd() helper) */
	int (*prime_handle_to_fd)(struct drm_device *dev, struct drm_file *file_priv,
				uint32_t handle, uint32_t flags, int *prime_fd);
	/* import fd -> handle (see drm_gem_prime_fd_to_handle() helper) */
	int (*prime_fd_to_handle)(struct drm_device *dev, struct drm_file *file_priv,
				int prime_fd, uint32_t *handle);
	/* export GEM -> dmabuf */
	struct dma_buf * (*gem_prime_export)(struct drm_device *dev,
				struct drm_gem_object *obj, int flags);
	/* import dmabuf -> GEM */
	struct drm_gem_object * (*gem_prime_import)(struct drm_device *dev,
				struct dma_buf *dma_buf);
	/* low-level interface used by drm_gem_prime_{import,export} */
	int (*gem_prime_pin)(struct drm_gem_object *obj);
	void (*gem_prime_unpin)(struct drm_gem_object *obj);
	struct reservation_object * (*gem_prime_res_obj)(
				struct drm_gem_object *obj);
	struct sg_table *(*gem_prime_get_sg_table)(struct drm_gem_object *obj);
	struct drm_gem_object *(*gem_prime_import_sg_table)(
				struct drm_device *dev,
				struct dma_buf_attachment *attach,
				struct sg_table *sgt);
	void *(*gem_prime_vmap)(struct drm_gem_object *obj);
	void (*gem_prime_vunmap)(struct drm_gem_object *obj, void *vaddr);
	int (*gem_prime_mmap)(struct drm_gem_object *obj,
				struct vm_area_struct *vma);

	/* vga arb irq handler */
	void (*vgaarb_irq)(struct drm_device *dev, bool state);

	/* dumb alloc support */
	int (*dumb_create)(struct drm_file *file_priv,
			   struct drm_device *dev,
			   struct drm_mode_create_dumb *args);
	int (*dumb_map_offset)(struct drm_file *file_priv,
			       struct drm_device *dev, uint32_t handle,
			       uint64_t *offset);
	int (*dumb_destroy)(struct drm_file *file_priv,
			    struct drm_device *dev,
			    uint32_t handle);

	/* Driver private ops for this object */
	struct cdev_pager_ops *gem_vm_ops;

	int major;
	int minor;
	int patchlevel;
	char *name;
	char *desc;
	char *date;

	u32 driver_features;
	int dev_priv_size;
	const struct drm_ioctl_desc *ioctls;
	int num_ioctls;
	const struct file_operations *fops;

	/* List of devices hanging off this driver with stealth attach. */
	struct list_head legacy_dev_list;
#ifdef __DragonFly__
	int (*sysctl_init) (struct drm_device *dev,
		    struct sysctl_ctx_list *ctx, struct sysctl_oid *top);
	void (*sysctl_cleanup) (struct drm_device *dev);
#endif	/* __DragonFly__ */
};

enum drm_minor_type {
	DRM_MINOR_LEGACY,
	DRM_MINOR_CONTROL,
	DRM_MINOR_RENDER,
	DRM_MINOR_CNT,
};

/**
 * Info file list entry. This structure represents a debugfs or proc file to
 * be created by the drm core
 */
struct drm_info_list {
	const char *name; /** file name */
	int (*show)(struct seq_file*, void*); /** show callback */
	u32 driver_features; /**< Required driver features for this entry */
	void *data;
};

/**
 * debugfs node structure. This structure represents a debugfs file.
 */
struct drm_info_node {
	struct list_head list;
	struct drm_minor *minor;
	const struct drm_info_list *info_ent;
	struct dentry *dent;
};

/**
 * DRM minor structure. This structure represents a drm minor number.
 */
struct drm_minor {
	int index;			/**< Minor device number */
	int type;                       /**< Control or render */
	struct device *kdev;		/**< Linux device */
	struct drm_device *dev;

	struct dentry *debugfs_root;

	struct list_head debugfs_list;
	struct lock debugfs_lock; /* Protects debugfs_list. */

	/* currently active master for this node. Protected by master_mutex */
	struct drm_master *master;
};


struct drm_pending_vblank_event {
	struct drm_pending_event base;
	unsigned int pipe;
	struct drm_event_vblank event;
};

struct drm_vblank_crtc {
	struct drm_device *dev;		/* pointer to the drm_device */
	wait_queue_head_t queue;	/**< VBLANK wait queue */
	struct timer_list disable_timer;		/* delayed disable timer */

	/* vblank counter, protected by dev->vblank_time_lock for writes */
	u32 count;
	/* vblank timestamps, protected by dev->vblank_time_lock for writes */
	struct timeval time[DRM_VBLANKTIME_RBSIZE];

	atomic_t refcount;		/* number of users of vblank interruptsper crtc */
	u32 last;			/* protected by dev->vbl_lock, used */
					/* for wraparound handling */
	u32 last_wait;			/* Last vblank seqno waited per CRTC */
	unsigned int inmodeset;		/* Display driver is setting mode */
	unsigned int pipe;		/* crtc index */
	int framedur_ns;		/* frame/field duration in ns */
	int linedur_ns;			/* line duration in ns */
	bool enabled;			/* so we don't call enable more than
					   once per disable */
};

struct drm_sysctl_info {
	struct sysctl_ctx_list ctx;
	char   name[2];
};

/* Length for the array of resource pointers for drm_get_resource_*. */
#define DRM_MAX_PCI_RESOURCE	6

/**
 * DRM device structure. This structure represent a complete card that
 * may contain multiple heads.
 */
struct drm_device {
	struct list_head legacy_dev_list;/**< list of devices per driver for stealth attach cleanup */
	int if_version;			/**< Highest interface version set */

	int		  unique_len;	/* Length of unique field	   */
	struct cdev	  *devnode;	/* Device number for mknod	   */

	int		  flags;	/* Flags to open(2)		   */

	struct lwkt_serialize irq_lock;	/* protects irq condition checks */
	struct lock	  dev_lock;	/* protects everything else */

	/** \name Lifetime Management */
	/*@{ */
	struct kref ref;		/**< Object ref-count */
	struct device *dev;		/**< Device structure of bus-device */
	struct drm_driver *driver;	/**< DRM driver managing the device */
	void *dev_private;		/**< DRM driver private data */
	struct drm_minor *control;		/**< Control node */
	struct drm_minor *primary;		/**< Primary node */
	struct drm_minor *render;		/**< Render node */
	atomic_t unplugged;			/**< Flag whether dev is dead */
	struct inode *anon_inode;		/**< inode for private address-space */
	char *unique;				/**< unique name of the device */
	/*@} */

	/** \name Locks */
	/*@{ */
	struct lock struct_mutex;	/**< For others */
	struct lock master_mutex;      /**< For drm_minor::master and drm_file::is_master */
	/*@} */

	/** \name Usage Counters */
	/*@{ */
	int open_count;			/**< Outstanding files open, protected by drm_global_mutex. */
	struct spinlock buf_lock;	/**< For drm_device::buf_use and a few other things. */
	int buf_use;			/**< Buffers in use -- cannot alloc */
	atomic_t buf_alloc;		/**< Buffer allocation in progress */
	/*@} */

	/** \name Performance counters */
	/*@{ */
	unsigned long     counters;
	enum drm_stat_type	types[15];
	atomic_t          counts[15];
	/*@} */

	struct idr magic_map;

	struct lock filelist_mutex;
	struct list_head filelist;

	/** \name Memory management */
	/*@{ */
	struct list_head maplist;	/**< Linked list of regions */
	struct drm_open_hash map_hash;	/**< User token hash table for maps */

	/** \name Context handle management */
	/*@{ */
	struct list_head ctxlist;	/**< Linked list of context handles */
	struct lock ctxlist_mutex;	/**< For ctxlist */

	struct idr ctx_idr;

	struct list_head vmalist;	/**< List of vmas (for debugging) */

	/*@} */

	struct drm_lock_data lock;	/* Information on hardware lock	   */

	/** \name DMA support */
	/*@{ */
	struct drm_device_dma *dma;		/**< Optional pointer for DMA support */
	/*@} */

	/** \name Context support */
	/*@{ */

	__volatile__ long context_flag;	/**< Context swapping flag */
	int last_context;		/**< Last current context */
	/*@} */

	/** \name VBLANK IRQ support */
	/*@{ */
	bool irq_enabled;
	int irq;

	/*
	 * If true, vblank interrupt will be disabled immediately when the
	 * refcount drops to zero, as opposed to via the vblank disable
	 * timer.
	 * This can be set to true it the hardware has a working vblank
	 * counter and the driver uses drm_vblank_on() and drm_vblank_off()
	 * appropriately.
	 */
	bool vblank_disable_immediate;

	/* array of size num_crtcs */
	struct drm_vblank_crtc *vblank;

	struct lock vblank_time_lock;    /**< Protects vblank count and time updates during vblank enable/disable */
	struct lock vbl_lock;

	u32 max_vblank_count;           /**< size of vblank counter register */

	/**
	 * List of events
	 */
	struct list_head vblank_event_list;
	struct lock event_lock;

	/*@} */

	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */
	struct drm_agp_head *agp;	/**< AGP data */

	struct pci_dev *pdev;		/**< PCI device structure */
#ifdef __alpha__
	struct pci_controller *hose;
#endif

	struct platform_device *platformdev; /**< Platform device struture */
	struct virtio_device *virtdev;

	struct drm_sg_mem *sg;	/**< Scatter gather memory */
	unsigned int num_crtcs;                  /**< Number of CRTCs on this device */

	void		  *drm_ttm_bdev;

	struct {
		int context;
		struct drm_hw_lock *lock;
	} sigdata;

	struct drm_local_map *agp_buffer_map;
	unsigned int agp_buffer_token;

	struct drm_mode_config mode_config;	/**< Current mode config */

	/** \name GEM information */
	/*@{ */
	struct lock object_name_lock;
	struct idr object_name_idr;
	struct drm_vma_offset_manager *vma_offset_manager;
	/*@} */
	int switch_power_state;
#ifdef __DragonFly__
	/* Storage of resource pointers for drm_get_resource_* */
	struct resource   *pcir[DRM_MAX_PCI_RESOURCE];
	int		  pcirid[DRM_MAX_PCI_RESOURCE];

	int		  pci_domain;
	int		  pci_bus;
	int		  pci_slot;
	int		  pci_func;
	drm_pci_id_list_t *id_entry;	/* PCI ID, name, and chipset private */
	char busid_str[128];
	int modesetting;
	void             *mm_private;
	struct drm_sysctl_info *sysctl;
#endif
};

#define DRM_SWITCH_POWER_ON 0
#define DRM_SWITCH_POWER_OFF 1
#define DRM_SWITCH_POWER_CHANGING 2
#define DRM_SWITCH_POWER_DYNAMIC_OFF 3

static __inline__ int drm_core_check_feature(struct drm_device *dev,
					     int feature)
{
	return ((dev->driver->driver_features & feature) ? 1 : 0);
}

static inline void drm_device_set_unplugged(struct drm_device *dev)
{
	smp_wmb();
	atomic_set(&dev->unplugged, 1);
}

static inline int drm_device_is_unplugged(struct drm_device *dev)
{
	int ret = atomic_read(&dev->unplugged);
	smp_rmb();
	return ret;
}

static inline bool drm_is_render_client(const struct drm_file *file_priv)
{
	return file_priv->minor->type == DRM_MINOR_RENDER;
}

static inline bool drm_is_control_client(const struct drm_file *file_priv)
{
	return file_priv->minor->type == DRM_MINOR_CONTROL;
}

static inline bool drm_is_primary_client(const struct drm_file *file_priv)
{
	return file_priv->minor->type == DRM_MINOR_LEGACY;
}

/******************************************************************/
/** \name Internal function definitions */
/*@{*/

				/* Driver support (drm_drv.h) */
extern int drm_ioctl_permit(u32 flags, struct drm_file *file_priv);
int	drm_probe(device_t kdev, drm_pci_id_list_t *idlist);
int	drm_attach(device_t kdev, drm_pci_id_list_t *idlist);
int	drm_create_cdevs(device_t kdev);
d_ioctl_t drm_ioctl;
extern long drm_compat_ioctl(struct file *filp,
			     unsigned int cmd, unsigned long arg);
extern bool drm_ioctl_flags(unsigned int nr, unsigned int *flags);

/* File Operations (drm_fops.c) */
d_open_t drm_open;
d_close_t drm_close;
d_read_t drm_read;
d_kqfilter_t drm_kqfilter;
int drm_release(device_t kdev);

d_mmap_t drm_mmap;
d_mmap_single_t drm_mmap_single;

void drm_cdevpriv_dtor(void *cd);

int drm_add_busid_modesetting(struct drm_device *dev,
    struct sysctl_ctx_list *ctx, struct sysctl_oid *top);

/* File operations helpers (drm_fops.c) */
extern int		drm_open_helper(struct cdev *kdev, int flags, int fmt,
					 DRM_STRUCTPROC *p,
					struct drm_device *dev,
					struct file *fp);

int drm_new_set_master(struct drm_device *dev, struct drm_file *fpriv);
int drm_event_reserve_init_locked(struct drm_device *dev,
				  struct drm_file *file_priv,
				  struct drm_pending_event *p,
				  struct drm_event *e);
int drm_event_reserve_init(struct drm_device *dev,
			   struct drm_file *file_priv,
			   struct drm_pending_event *p,
			   struct drm_event *e);
void drm_event_cancel_free(struct drm_device *dev,
			   struct drm_pending_event *p);
void drm_send_event_locked(struct drm_device *dev, struct drm_pending_event *e);
void drm_send_event(struct drm_device *dev, struct drm_pending_event *e);

/* Misc. IOCTL support (drm_ioctl.c) */
int drm_noop(struct drm_device *dev, void *data,
	     struct drm_file *file_priv);
int drm_invalid_op(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);

/* Cache management (drm_cache.c) */
void drm_clflush_pages(struct page *pages[], unsigned long num_pages);
void drm_clflush_sg(struct sg_table *st);
void drm_clflush_virt_range(void *addr, unsigned long length);

/*
 * These are exported to drivers so that they can implement fencing using
 * DMA quiscent + idle. DMA quiescent usually requires the hardware lock.
 */

unsigned long drm_get_resource_start(struct drm_device *dev,
				     unsigned int resource);
unsigned long drm_get_resource_len(struct drm_device *dev,
				   unsigned int resource);

				/* IRQ support (drm_irq.h) */
extern int drm_irq_install(struct drm_device *dev, int irq);
extern int drm_irq_uninstall(struct drm_device *dev);

extern int drm_vblank_init(struct drm_device *dev, unsigned int num_crtcs);
extern int drm_wait_vblank(struct drm_device *dev, void *data,
			   struct drm_file *filp);
extern u32 drm_vblank_count(struct drm_device *dev, unsigned int pipe);
extern u32 drm_crtc_vblank_count(struct drm_crtc *crtc);
extern u32 drm_vblank_count_and_time(struct drm_device *dev, unsigned int pipe,
				     struct timeval *vblanktime);
extern u32 drm_crtc_vblank_count_and_time(struct drm_crtc *crtc,
					  struct timeval *vblanktime);
extern void drm_send_vblank_event(struct drm_device *dev, unsigned int pipe,
				  struct drm_pending_vblank_event *e);
extern void drm_crtc_send_vblank_event(struct drm_crtc *crtc,
				       struct drm_pending_vblank_event *e);
extern void drm_arm_vblank_event(struct drm_device *dev, unsigned int pipe,
				 struct drm_pending_vblank_event *e);
extern void drm_crtc_arm_vblank_event(struct drm_crtc *crtc,
				      struct drm_pending_vblank_event *e);
extern bool drm_handle_vblank(struct drm_device *dev, unsigned int pipe);
extern bool drm_crtc_handle_vblank(struct drm_crtc *crtc);
extern int drm_vblank_get(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_put(struct drm_device *dev, unsigned int pipe);
extern int drm_crtc_vblank_get(struct drm_crtc *crtc);
extern void drm_crtc_vblank_put(struct drm_crtc *crtc);
extern void drm_wait_one_vblank(struct drm_device *dev, unsigned int pipe);
extern void drm_crtc_wait_one_vblank(struct drm_crtc *crtc);
extern void drm_vblank_off(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_on(struct drm_device *dev, unsigned int pipe);
extern void drm_crtc_vblank_off(struct drm_crtc *crtc);
extern void drm_crtc_vblank_reset(struct drm_crtc *crtc);
extern void drm_crtc_vblank_on(struct drm_crtc *crtc);
extern void drm_vblank_cleanup(struct drm_device *dev);
extern u32 drm_vblank_no_hw_counter(struct drm_device *dev, unsigned int pipe);

extern int drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev,
						 unsigned int pipe, int *max_error,
						 struct timeval *vblank_time,
						 unsigned flags,
						 const struct drm_display_mode *mode);
extern void drm_calc_timestamping_constants(struct drm_crtc *crtc,
					    const struct drm_display_mode *mode);

/**
 * drm_crtc_vblank_waitqueue - get vblank waitqueue for the CRTC
 * @crtc: which CRTC's vblank waitqueue to retrieve
 *
 * This function returns a pointer to the vblank waitqueue for the CRTC.
 * Drivers can use this to implement vblank waits using wait_event() & co.
 */
static inline wait_queue_head_t *drm_crtc_vblank_waitqueue(struct drm_crtc *crtc)
{
	return &crtc->dev->vblank[drm_crtc_index(crtc)].queue;
}

/* Modesetting support */
extern void drm_vblank_pre_modeset(struct drm_device *dev, unsigned int pipe);
extern void drm_vblank_post_modeset(struct drm_device *dev, unsigned int pipe);

				/* Stub support (drm_stub.h) */
extern struct drm_master *drm_master_get(struct drm_master *master);
extern void drm_master_put(struct drm_master **master);

extern void drm_put_dev(struct drm_device *dev);
extern void drm_unplug_dev(struct drm_device *dev);
extern unsigned int drm_debug;
extern bool drm_atomic;

				/* Debugfs support */
#if defined(CONFIG_DEBUG_FS)
extern int drm_debugfs_create_files(const struct drm_info_list *files,
				    int count, struct dentry *root,
				    struct drm_minor *minor);
extern int drm_debugfs_remove_files(const struct drm_info_list *files,
				    int count, struct drm_minor *minor);
#else
static inline int drm_debugfs_create_files(const struct drm_info_list *files,
					   int count, struct dentry *root,
					   struct drm_minor *minor)
{
	return 0;
}

static inline int drm_debugfs_remove_files(const struct drm_info_list *files,
					   int count, struct drm_minor *minor)
{
	return 0;
}
#endif

/* sysctl support (drm_sysctl.h) */
extern int drm_sysctl_init(struct drm_device *dev);
extern int drm_sysctl_cleanup(struct drm_device *dev);

/* XXX: These are here only because of drm_sysctl.c */
extern int drm_vblank_offdelay;
extern unsigned int drm_timestamp_precision;

int drm_gem_mmap_single(struct drm_device *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **obj_res, int nprot);

extern struct dma_buf *drm_gem_prime_export(struct drm_device *dev,
		struct drm_gem_object *obj, int flags);
extern int drm_gem_prime_handle_to_fd(struct drm_device *dev,
		struct drm_file *file_priv, uint32_t handle, uint32_t flags,
		int *prime_fd);
extern struct drm_gem_object *drm_gem_prime_import(struct drm_device *dev,
		struct dma_buf *dma_buf);
extern int drm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle);
extern void drm_gem_dmabuf_release(struct dma_buf *dma_buf);

extern int drm_prime_sg_to_page_addr_arrays(struct sg_table *sgt, struct page **pages,
					    dma_addr_t *addrs, int max_pages);
extern struct sg_table *drm_prime_pages_to_sg(struct page **pages, unsigned int nr_pages);
extern void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg);


extern struct drm_dma_handle *drm_pci_alloc(struct drm_device *dev, size_t size,
					    size_t align);
extern void drm_pci_free(struct drm_device *dev, struct drm_dma_handle * dmah);

			       /* sysfs support (drm_sysfs.c) */
extern void drm_sysfs_hotplug_event(struct drm_device *dev);

struct ttm_bo_device;
int ttm_bo_mmap_single(struct ttm_bo_device *bdev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **obj_res, int nprot);
struct ttm_buffer_object;
void ttm_bo_release_mmap(struct ttm_buffer_object *bo);

/* simplified version of kvasnprintf() for drm needs. */
char *drm_vasprintf(int flags, const char *format, __va_list ap) __printflike(2, 0);
char *drm_asprintf(int flags, const char *format, ...) __printflike(2, 3);

/* XXX glue logic, should be done in drm_pci_init(), pending drm update */
void drm_init_pdev(device_t dev, struct pci_dev **pdev);
void drm_fini_pdev(struct pci_dev **pdev);
void drm_print_pdev(struct pci_dev *pdev);

/* Inline drm_free() helper for area kfree() */
static __inline__ void
drm_free(void *pt, struct malloc_type *area)
{
	/* kfree is special!!! */
	if (pt != NULL)
		(kfree)(pt, area);
}

struct drm_device *drm_dev_alloc(struct drm_driver *driver,
				 struct device *parent);
void drm_dev_ref(struct drm_device *dev);
void drm_dev_unref(struct drm_device *dev);
int drm_dev_register(struct drm_device *dev, unsigned long flags);
void drm_dev_unregister(struct drm_device *dev);
int drm_dev_set_unique(struct drm_device *dev, const char *name);

struct drm_minor *drm_minor_acquire(unsigned int minor_id);
void drm_minor_release(struct drm_minor *minor);

/*@}*/

/* PCI section */
static __inline__ int drm_pci_device_is_agp(struct drm_device *dev)
{
	if (dev->driver->device_is_agp != NULL) {
		int err = (*dev->driver->device_is_agp) (dev);

		if (err != 2) {
			return err;
		}
	}

	return (pci_find_extcap(dev->pdev->dev.bsddev, PCIY_AGP, NULL) == 0);
}
void drm_pci_agp_destroy(struct drm_device *dev);

extern int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver);
extern void drm_pci_exit(struct drm_driver *driver, struct pci_driver *pdriver);
#ifdef CONFIG_PCI
extern int drm_get_pci_dev(struct pci_dev *pdev,
			   const struct pci_device_id *ent,
			   struct drm_driver *driver);
extern int drm_pci_set_busid(struct drm_device *dev, struct drm_master *master);
#else
static inline int drm_get_pci_dev(struct pci_dev *pdev,
				  const struct pci_device_id *ent,
				  struct drm_driver *driver)
{
	return -ENOSYS;
}

static inline int drm_pci_set_busid(struct drm_device *dev,
				    struct drm_master *master)
{
	return -ENOSYS;
}
#endif

#define DRM_PCIE_SPEED_25 1
#define DRM_PCIE_SPEED_50 2
#define DRM_PCIE_SPEED_80 4

extern int drm_pcie_get_speed_cap_mask(struct drm_device *dev, u32 *speed_mask);
extern int drm_pcie_get_max_link_width(struct drm_device *dev, u32 *mlw);

/* platform section */
extern int drm_platform_init(struct drm_driver *driver, struct platform_device *platform_device);
extern int drm_platform_set_busid(struct drm_device *d, struct drm_master *m);

/* XXX bad */
#define	drm_can_sleep()	(HZ & 1)

#endif /* __KERNEL__ */

/* helper for handling conditionals in various for_each macros */
#define for_each_if(condition) if (!(condition)) {} else

#endif
