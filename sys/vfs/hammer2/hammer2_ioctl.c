/*
 * Copyright (c) 2011-2018 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Ioctl Functions.
 *
 * WARNING! The ioctl functions which manipulate the connection state need
 *	    to be able to run without deadlock on the volume's chain lock.
 *	    Most of these functions use a separate lock.
 */

#include "hammer2.h"

static int hammer2_ioctl_version_get(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_recluster(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_remote_scan(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_remote_add(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_remote_del(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_remote_rep(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_socket_get(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_socket_set(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_pfs_get(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_pfs_lookup(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_pfs_create(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_pfs_snapshot(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_pfs_delete(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_inode_get(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_inode_set(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_debug_dump(hammer2_inode_t *ip, u_int flags);
//static int hammer2_ioctl_inode_comp_set(hammer2_inode_t *ip, void *data);
//static int hammer2_ioctl_inode_comp_rec_set(hammer2_inode_t *ip, void *data);
//static int hammer2_ioctl_inode_comp_rec_set2(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_bulkfree_scan(hammer2_inode_t *ip, void *data);
static int hammer2_ioctl_destroy(hammer2_inode_t *ip, void *data);

int
hammer2_ioctl(hammer2_inode_t *ip, u_long com, void *data, int fflag,
	      struct ucred *cred)
{
	int error;

	/*
	 * Standard root cred checks, will be selectively ignored below
	 * for ioctls that do not require root creds.
	 */
	error = priv_check_cred(cred, PRIV_HAMMER_IOCTL, 0);

	switch(com) {
	case HAMMER2IOC_VERSION_GET:
		error = hammer2_ioctl_version_get(ip, data);
		break;
	case HAMMER2IOC_RECLUSTER:
		if (error == 0)
			error = hammer2_ioctl_recluster(ip, data);
		break;
	case HAMMER2IOC_REMOTE_SCAN:
		if (error == 0)
			error = hammer2_ioctl_remote_scan(ip, data);
		break;
	case HAMMER2IOC_REMOTE_ADD:
		if (error == 0)
			error = hammer2_ioctl_remote_add(ip, data);
		break;
	case HAMMER2IOC_REMOTE_DEL:
		if (error == 0)
			error = hammer2_ioctl_remote_del(ip, data);
		break;
	case HAMMER2IOC_REMOTE_REP:
		if (error == 0)
			error = hammer2_ioctl_remote_rep(ip, data);
		break;
	case HAMMER2IOC_SOCKET_GET:
		if (error == 0)
			error = hammer2_ioctl_socket_get(ip, data);
		break;
	case HAMMER2IOC_SOCKET_SET:
		if (error == 0)
			error = hammer2_ioctl_socket_set(ip, data);
		break;
	case HAMMER2IOC_PFS_GET:
		if (error == 0)
			error = hammer2_ioctl_pfs_get(ip, data);
		break;
	case HAMMER2IOC_PFS_LOOKUP:
		if (error == 0)
			error = hammer2_ioctl_pfs_lookup(ip, data);
		break;
	case HAMMER2IOC_PFS_CREATE:
		if (error == 0)
			error = hammer2_ioctl_pfs_create(ip, data);
		break;
	case HAMMER2IOC_PFS_DELETE:
		if (error == 0)
			error = hammer2_ioctl_pfs_delete(ip, data);
		break;
	case HAMMER2IOC_PFS_SNAPSHOT:
		if (error == 0)
			error = hammer2_ioctl_pfs_snapshot(ip, data);
		break;
	case HAMMER2IOC_INODE_GET:
		error = hammer2_ioctl_inode_get(ip, data);
		break;
	case HAMMER2IOC_INODE_SET:
		if (error == 0)
			error = hammer2_ioctl_inode_set(ip, data);
		break;
	case HAMMER2IOC_BULKFREE_SCAN:
		error = hammer2_ioctl_bulkfree_scan(ip, data);
		break;
	case HAMMER2IOC_BULKFREE_ASYNC:
		error = hammer2_ioctl_bulkfree_scan(ip, NULL);
		break;
	/*case HAMMER2IOC_INODE_COMP_SET:
		error = hammer2_ioctl_inode_comp_set(ip, data);
		break;
	case HAMMER2IOC_INODE_COMP_REC_SET:
	 	error = hammer2_ioctl_inode_comp_rec_set(ip, data);
	 	break;
	case HAMMER2IOC_INODE_COMP_REC_SET2:
		error = hammer2_ioctl_inode_comp_rec_set2(ip, data);
		break;*/
	case HAMMER2IOC_DESTROY:
		if (error == 0)
			error = hammer2_ioctl_destroy(ip, data);
		break;
	case HAMMER2IOC_DEBUG_DUMP:
		error = hammer2_ioctl_debug_dump(ip, *(u_int *)data);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

/*
 * Retrieve version and basic info
 */
static int
hammer2_ioctl_version_get(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_version_t *version = data;
	hammer2_dev_t *hmp;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp)
		version->version = hmp->voldata.version;
	else
		version->version = -1;
	return 0;
}

static int
hammer2_ioctl_recluster(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_recluster_t *recl = data;
	struct vnode *vproot;
	struct file *fp;
	hammer2_cluster_t *cluster;
	int error;

	fp = holdfp(curthread, recl->fd, -1);
	if (fp) {
		error = VFS_ROOT(ip->pmp->mp, &vproot);
		if (error == 0) {
			cluster = &ip->pmp->iroot->cluster;
			kprintf("reconnect to cluster: nc=%d focus=%p\n",
				cluster->nchains, cluster->focus);
			if (cluster->nchains != 1 || cluster->focus == NULL) {
				kprintf("not a local device mount\n");
				error = EINVAL;
			} else {
				hammer2_cluster_reconnect(cluster->focus->hmp,
							  fp);
				kprintf("ok\n");
				error = 0;
			}
			vput(vproot);
		}
	} else {
		error = EINVAL;
	}
	return error;
}

/*
 * Retrieve information about a remote
 */
static int
hammer2_ioctl_remote_scan(hammer2_inode_t *ip, void *data)
{
	hammer2_dev_t *hmp;
	hammer2_ioc_remote_t *remote = data;
	int copyid = remote->copyid;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	if (copyid < 0 || copyid >= HAMMER2_COPYID_COUNT)
		return (EINVAL);

	hammer2_voldata_lock(hmp);
	remote->copy1 = hmp->voldata.copyinfo[copyid];
	hammer2_voldata_unlock(hmp);

	/*
	 * Adjust nextid (GET only)
	 */
	while (++copyid < HAMMER2_COPYID_COUNT &&
	       hmp->voldata.copyinfo[copyid].copyid == 0) {
		;
	}
	if (copyid == HAMMER2_COPYID_COUNT)
		remote->nextid = -1;
	else
		remote->nextid = copyid;

	return(0);
}

/*
 * Add new remote entry
 */
static int
hammer2_ioctl_remote_add(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_remote_t *remote = data;
	hammer2_pfs_t *pmp = ip->pmp;
	hammer2_dev_t *hmp;
	int copyid = remote->copyid;
	int error = 0;

	hmp = pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);
	if (copyid >= HAMMER2_COPYID_COUNT)
		return (EINVAL);

	hammer2_voldata_lock(hmp);
	if (copyid < 0) {
		for (copyid = 1; copyid < HAMMER2_COPYID_COUNT; ++copyid) {
			if (hmp->voldata.copyinfo[copyid].copyid == 0)
				break;
		}
		if (copyid == HAMMER2_COPYID_COUNT) {
			error = ENOSPC;
			goto failed;
		}
	}
	hammer2_voldata_modify(hmp);
	remote->copy1.copyid = copyid;
	hmp->voldata.copyinfo[copyid] = remote->copy1;
	hammer2_volconf_update(hmp, copyid);
failed:
	hammer2_voldata_unlock(hmp);
	return (error);
}

/*
 * Delete existing remote entry
 */
static int
hammer2_ioctl_remote_del(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_remote_t *remote = data;
	hammer2_pfs_t *pmp = ip->pmp;
	hammer2_dev_t *hmp;
	int copyid = remote->copyid;
	int error = 0;

	hmp = pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);
	if (copyid >= HAMMER2_COPYID_COUNT)
		return (EINVAL);
	remote->copy1.path[sizeof(remote->copy1.path) - 1] = 0;
	hammer2_voldata_lock(hmp);
	if (copyid < 0) {
		for (copyid = 1; copyid < HAMMER2_COPYID_COUNT; ++copyid) {
			if (hmp->voldata.copyinfo[copyid].copyid == 0)
				continue;
			if (strcmp(remote->copy1.path,
			    hmp->voldata.copyinfo[copyid].path) == 0) {
				break;
			}
		}
		if (copyid == HAMMER2_COPYID_COUNT) {
			error = ENOENT;
			goto failed;
		}
	}
	hammer2_voldata_modify(hmp);
	hmp->voldata.copyinfo[copyid].copyid = 0;
	hammer2_volconf_update(hmp, copyid);
failed:
	hammer2_voldata_unlock(hmp);
	return (error);
}

/*
 * Replace existing remote entry
 */
static int
hammer2_ioctl_remote_rep(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_remote_t *remote = data;
	hammer2_dev_t *hmp;
	int copyid = remote->copyid;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);
	if (copyid < 0 || copyid >= HAMMER2_COPYID_COUNT)
		return (EINVAL);

	hammer2_voldata_lock(hmp);
	hammer2_voldata_modify(hmp);
	/*hammer2_volconf_update(hmp, copyid);*/
	hammer2_voldata_unlock(hmp);

	return(0);
}

/*
 * Retrieve communications socket
 */
static int
hammer2_ioctl_socket_get(hammer2_inode_t *ip, void *data)
{
	return (EOPNOTSUPP);
}

/*
 * Set communications socket for connection
 */
static int
hammer2_ioctl_socket_set(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_remote_t *remote = data;
	hammer2_dev_t *hmp;
	int copyid = remote->copyid;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);
	if (copyid < 0 || copyid >= HAMMER2_COPYID_COUNT)
		return (EINVAL);

	hammer2_voldata_lock(hmp);
	hammer2_voldata_unlock(hmp);

	return(0);
}

/*
 * Used to scan and retrieve PFS information.  PFS's are directories under
 * the super-root.
 *
 * To scan PFSs pass name_key=0.  The function will scan for the next
 * PFS and set all fields, as well as set name_next to the next key.
 * When no PFSs remain, name_next is set to (hammer2_key_t)-1.
 *
 * To retrieve a particular PFS by key, specify the key but note that
 * the ioctl will return the lowest key >= specified_key, so the caller
 * must verify the key.
 *
 * To retrieve the PFS associated with the file descriptor, pass
 * name_key set to (hammer2_key_t)-1.
 */
static int
hammer2_ioctl_pfs_get(hammer2_inode_t *ip, void *data)
{
	const hammer2_inode_data_t *ripdata;
	hammer2_dev_t *hmp;
	hammer2_ioc_pfs_t *pfs;
	hammer2_chain_t *parent;
	hammer2_chain_t *chain;
	hammer2_key_t key_next;
	hammer2_key_t save_key;
	int error;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	pfs = data;
	save_key = pfs->name_key;
	error = 0;

	/*
	 * Setup
	 */
	if (save_key == (hammer2_key_t)-1) {
		hammer2_inode_lock(ip->pmp->iroot, 0);
		parent = NULL;
		chain = hammer2_inode_chain(ip->pmp->iroot, 0,
					    HAMMER2_RESOLVE_ALWAYS |
					    HAMMER2_RESOLVE_SHARED);
	} else {
		hammer2_inode_lock(hmp->spmp->iroot, 0);
		parent = hammer2_inode_chain(hmp->spmp->iroot, 0,
					    HAMMER2_RESOLVE_ALWAYS |
					    HAMMER2_RESOLVE_SHARED);
		chain = hammer2_chain_lookup(&parent, &key_next,
					    pfs->name_key, HAMMER2_KEY_MAX,
					    &error,
					    HAMMER2_LOOKUP_SHARED);
	}

	/*
	 * Locate next PFS
	 */
	while (chain) {
		if (chain->bref.type == HAMMER2_BREF_TYPE_INODE)
			break;
		if (parent == NULL) {
			hammer2_chain_unlock(chain);
			hammer2_chain_drop(chain);
			chain = NULL;
			break;
		}
		chain = hammer2_chain_next(&parent, chain, &key_next,
					    key_next, HAMMER2_KEY_MAX,
					    &error,
					    HAMMER2_LOOKUP_SHARED);
	}
	error = hammer2_error_to_errno(error);

	/*
	 * Load the data being returned by the ioctl.
	 */
	if (chain && chain->error == 0) {
		ripdata = &chain->data->ipdata;
		pfs->name_key = ripdata->meta.name_key;
		pfs->pfs_type = ripdata->meta.pfs_type;
		pfs->pfs_subtype = ripdata->meta.pfs_subtype;
		pfs->pfs_clid = ripdata->meta.pfs_clid;
		pfs->pfs_fsid = ripdata->meta.pfs_fsid;
		KKASSERT(ripdata->meta.name_len < sizeof(pfs->name));
		bcopy(ripdata->filename, pfs->name, ripdata->meta.name_len);
		pfs->name[ripdata->meta.name_len] = 0;
		ripdata = NULL;	/* safety */

		/*
		 * Calculate name_next, if any.  We are only accessing
		 * chain->bref so we can ignore chain->error (if the key
		 * is used later it will error then).
		 */
		if (parent == NULL) {
			pfs->name_next = (hammer2_key_t)-1;
		} else {
			chain = hammer2_chain_next(&parent, chain, &key_next,
						    key_next, HAMMER2_KEY_MAX,
						    &error,
						    HAMMER2_LOOKUP_SHARED);
			if (chain)
				pfs->name_next = chain->bref.key;
			else
				pfs->name_next = (hammer2_key_t)-1;
		}
	} else {
		pfs->name_next = (hammer2_key_t)-1;
		error = ENOENT;
	}

	/*
	 * Cleanup
	 */
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
	if (save_key == (hammer2_key_t)-1) {
		hammer2_inode_unlock(ip->pmp->iroot);
	} else {
		hammer2_inode_unlock(hmp->spmp->iroot);
	}

	return (error);
}

/*
 * Find a specific PFS by name
 */
static int
hammer2_ioctl_pfs_lookup(hammer2_inode_t *ip, void *data)
{
	const hammer2_inode_data_t *ripdata;
	hammer2_dev_t *hmp;
	hammer2_ioc_pfs_t *pfs;
	hammer2_chain_t *parent;
	hammer2_chain_t *chain;
	hammer2_key_t key_next;
	hammer2_key_t lhc;
	int error;
	size_t len;

	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	pfs = data;
	error = 0;

	hammer2_inode_lock(hmp->spmp->iroot, HAMMER2_RESOLVE_SHARED);
	parent = hammer2_inode_chain(hmp->spmp->iroot, 0,
				     HAMMER2_RESOLVE_ALWAYS |
				     HAMMER2_RESOLVE_SHARED);

	pfs->name[sizeof(pfs->name) - 1] = 0;
	len = strlen(pfs->name);
	lhc = hammer2_dirhash(pfs->name, len);

	chain = hammer2_chain_lookup(&parent, &key_next,
					 lhc, lhc + HAMMER2_DIRHASH_LOMASK,
					 &error, HAMMER2_LOOKUP_SHARED);
	while (chain) {
		if (hammer2_chain_dirent_test(chain, pfs->name, len))
			break;
		chain = hammer2_chain_next(&parent, chain, &key_next,
					   key_next,
					   lhc + HAMMER2_DIRHASH_LOMASK,
					   &error, HAMMER2_LOOKUP_SHARED);
	}
	error = hammer2_error_to_errno(error);

	/*
	 * Load the data being returned by the ioctl.
	 */
	if (chain && chain->error == 0) {
		KKASSERT(chain->bref.type == HAMMER2_BREF_TYPE_INODE);
		ripdata = &chain->data->ipdata;
		pfs->name_key = ripdata->meta.name_key;
		pfs->pfs_type = ripdata->meta.pfs_type;
		pfs->pfs_subtype = ripdata->meta.pfs_subtype;
		pfs->pfs_clid = ripdata->meta.pfs_clid;
		pfs->pfs_fsid = ripdata->meta.pfs_fsid;
		ripdata = NULL;

		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	} else if (error == 0) {
		error = ENOENT;
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
	hammer2_inode_unlock(hmp->spmp->iroot);

	return (error);
}

/*
 * Create a new PFS under the super-root
 */
static int
hammer2_ioctl_pfs_create(hammer2_inode_t *ip, void *data)
{
	hammer2_inode_data_t *nipdata;
	hammer2_chain_t *nchain;
	hammer2_dev_t *hmp;
	hammer2_dev_t *force_local;
	hammer2_ioc_pfs_t *pfs;
	hammer2_inode_t *nip;
	hammer2_tid_t mtid;
	int error;

	hmp = ip->pmp->pfs_hmps[0];	/* XXX */
	if (hmp == NULL)
		return (EINVAL);

	pfs = data;
	nip = NULL;

	if (pfs->name[0] == 0)
		return(EINVAL);
	pfs->name[sizeof(pfs->name) - 1] = 0;	/* ensure 0-termination */

	if (hammer2_ioctl_pfs_lookup(ip, pfs) == 0)
		return(EEXIST);

	hammer2_trans_init(hmp->spmp, 0);
	mtid = hammer2_trans_sub(hmp->spmp);
	nip = hammer2_inode_create(hmp->spmp->iroot, hmp->spmp->iroot,
				   NULL, NULL,
				   pfs->name, strlen(pfs->name), 0,
				   1, HAMMER2_OBJTYPE_DIRECTORY, 0,
				   HAMMER2_INSERT_PFSROOT, &error);
	if (error == 0) {
		nip->flags |= HAMMER2_INODE_NOSIDEQ;
		hammer2_inode_modify(nip);
		nchain = hammer2_inode_chain(nip, 0, HAMMER2_RESOLVE_ALWAYS);
		error = hammer2_chain_modify(nchain, mtid, 0, 0);
		KKASSERT(error == 0);
		nipdata = &nchain->data->ipdata;

		nip->meta.pfs_type = pfs->pfs_type;
		nip->meta.pfs_subtype = pfs->pfs_subtype;
		nip->meta.pfs_clid = pfs->pfs_clid;
		nip->meta.pfs_fsid = pfs->pfs_fsid;
		nip->meta.op_flags |= HAMMER2_OPFLAG_PFSROOT;

		/*
		 * Set default compression and check algorithm.  This
		 * can be changed later.
		 *
		 * Do not allow compression on PFS's with the special name
		 * "boot", the boot loader can't decompress (yet).
		 */
		nip->meta.comp_algo =
			HAMMER2_ENC_ALGO(HAMMER2_COMP_NEWFS_DEFAULT);
		nip->meta.check_algo =
			HAMMER2_ENC_ALGO( HAMMER2_CHECK_XXHASH64);

		if (strcasecmp(pfs->name, "boot") == 0) {
			nip->meta.comp_algo =
				HAMMER2_ENC_ALGO(HAMMER2_COMP_AUTOZERO);
		}

		/*
		 * Super-root isn't mounted, fsync it
		 */
		hammer2_chain_unlock(nchain);
		hammer2_inode_ref(nip);
		hammer2_inode_unlock(nip);
		hammer2_inode_chain_sync(nip);
		hammer2_inode_chain_flush(nip);
		KKASSERT(nip->refs == 1);
		hammer2_inode_drop(nip);

		/* 
		 * We still have a ref on the chain, relock and associate
		 * with an appropriate PFS.
		 */
		force_local = (hmp->hflags & HMNT2_LOCAL) ? hmp : NULL;

		hammer2_chain_lock(nchain, HAMMER2_RESOLVE_ALWAYS);
		nipdata = &nchain->data->ipdata;
		kprintf("ADD LOCAL PFS (IOCTL): %s\n", nipdata->filename);
		hammer2_pfsalloc(nchain, nipdata,
				 nchain->bref.modify_tid, force_local);

		hammer2_chain_unlock(nchain);
		hammer2_chain_drop(nchain);

	}
	hammer2_trans_done(hmp->spmp, 1);

	return (error);
}

/*
 * Destroy an existing PFS under the super-root
 */
static int
hammer2_ioctl_pfs_delete(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_pfs_t *pfs = data;
	hammer2_dev_t	*hmp;
	hammer2_pfs_t	*spmp;
	hammer2_pfs_t	*pmp;
	hammer2_xop_unlink_t *xop;
	hammer2_inode_t *dip;
	hammer2_inode_t *iroot;
	int error;
	int i;

	/*
	 * The PFS should be probed, so we should be able to
	 * locate it.  We only delete the PFS from the
	 * specific H2 block device (hmp), not all of
	 * them.  We must remove the PFS from the cluster
	 * before we can destroy it.
	 */
	hmp = ip->pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	pfs->name[sizeof(pfs->name) - 1] = 0;	/* ensure termination */

	lockmgr(&hammer2_mntlk, LK_EXCLUSIVE);

	TAILQ_FOREACH(pmp, &hammer2_pfslist, mntentry) {
		for (i = 0; i < HAMMER2_MAXCLUSTER; ++i) {
			if (pmp->pfs_hmps[i] != hmp)
				continue;
			if (pmp->pfs_names[i] &&
			    strcmp(pmp->pfs_names[i], pfs->name) == 0) {
				break;
			}
		}
		if (i != HAMMER2_MAXCLUSTER)
			break;
	}

	if (pmp == NULL) {
		lockmgr(&hammer2_mntlk, LK_RELEASE);
		return ENOENT;
	}

	/*
	 * Ok, we found the pmp and we have the index.  Permanently remove
	 * the PFS from the cluster
	 */
	iroot = pmp->iroot;
	kprintf("FOUND PFS %s CLINDEX %d\n", pfs->name, i);
	hammer2_pfsdealloc(pmp, i, 1);

	lockmgr(&hammer2_mntlk, LK_RELEASE);

	/*
	 * Now destroy the PFS under its device using the per-device
	 * super-root.
	 */
	spmp = hmp->spmp;
	dip = spmp->iroot;
	hammer2_trans_init(spmp, 0);
	hammer2_inode_lock(dip, 0);

	xop = hammer2_xop_alloc(dip, HAMMER2_XOP_MODIFYING);
	hammer2_xop_setname(&xop->head, pfs->name, strlen(pfs->name));
	xop->isdir = 2;
	xop->dopermanent = H2DOPERM_PERMANENT | H2DOPERM_FORCE;
	hammer2_xop_start(&xop->head, &hammer2_unlink_desc);

	error = hammer2_xop_collect(&xop->head, 0);

	hammer2_inode_unlock(dip);

#if 0
        if (error == 0) {
                ip = hammer2_inode_get(dip->pmp, dip, &xop->head, -1);
                hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
                if (ip) {
                        hammer2_inode_unlink_finisher(ip, 0);
                        hammer2_inode_unlock(ip);
                }
        } else {
                hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
        }
#endif
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);

	hammer2_trans_done(spmp, 1);

	return (hammer2_error_to_errno(error));
}

static int
hammer2_ioctl_pfs_snapshot(hammer2_inode_t *ip, void *data)
{
	const hammer2_inode_data_t *ripdata;
	hammer2_ioc_pfs_t *pfs = data;
	hammer2_dev_t	*hmp;
	hammer2_pfs_t	*pmp;
	hammer2_chain_t	*chain;
	hammer2_inode_t *nip;
	hammer2_tid_t	mtid;
	size_t name_len;
	hammer2_key_t lhc;
	struct vattr vat;
	int error;
#if 0
	uuid_t opfs_clid;
#endif

	if (pfs->name[0] == 0)
		return(EINVAL);
	if (pfs->name[sizeof(pfs->name)-1] != 0)
		return(EINVAL);

	pmp = ip->pmp;
	ip = pmp->iroot;

	hmp = pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	lockmgr(&hmp->bulklk, LK_EXCLUSIVE);

	hammer2_vfs_sync(pmp->mp, MNT_WAIT);

	hammer2_trans_init(pmp, HAMMER2_TRANS_ISFLUSH);
	mtid = hammer2_trans_sub(pmp);
	hammer2_inode_lock(ip, 0);
	hammer2_inode_modify(ip);
	ip->meta.pfs_lsnap_tid = mtid;

	/* XXX cluster it! */
	chain = hammer2_inode_chain(ip, 0, HAMMER2_RESOLVE_ALWAYS);

	name_len = strlen(pfs->name);
	lhc = hammer2_dirhash(pfs->name, name_len);

	/*
	 * Get the clid
	 */
	ripdata = &chain->data->ipdata;
#if 0
	opfs_clid = ripdata->meta.pfs_clid;
#endif
	hmp = chain->hmp;

	/*
	 * Create the snapshot directory under the super-root
	 *
	 * Set PFS type, generate a unique filesystem id, and generate
	 * a cluster id.  Use the same clid when snapshotting a PFS root,
	 * which theoretically allows the snapshot to be used as part of
	 * the same cluster (perhaps as a cache).
	 *
	 * Copy the (flushed) blockref array.  Theoretically we could use
	 * chain_duplicate() but it becomes difficult to disentangle
	 * the shared core so for now just brute-force it.
	 */
	VATTR_NULL(&vat);
	vat.va_type = VDIR;
	vat.va_mode = 0755;
	hammer2_chain_unlock(chain);
	nip = hammer2_inode_create(hmp->spmp->iroot, hmp->spmp->iroot,
				   &vat, proc0.p_ucred,
				   pfs->name, name_len, 0,
				   1, 0, 0,
				   HAMMER2_INSERT_PFSROOT, &error);
	hammer2_chain_lock(chain, HAMMER2_RESOLVE_ALWAYS);
	ripdata = &chain->data->ipdata;

	if (nip) {
		hammer2_dev_t *force_local;
		hammer2_chain_t *nchain;
		hammer2_inode_data_t *wipdata;
		hammer2_key_t	starting_inum;

		nip->flags |= HAMMER2_INODE_NOSIDEQ;
		hammer2_inode_modify(nip);
		nchain = hammer2_inode_chain(nip, 0, HAMMER2_RESOLVE_ALWAYS);
		error = hammer2_chain_modify(nchain, mtid, 0, 0);
		KKASSERT(error == 0);
		wipdata = &nchain->data->ipdata;

		starting_inum = ip->pmp->inode_tid + 1;
		nip->meta.pfs_inum = starting_inum;
		nip->meta.pfs_type = HAMMER2_PFSTYPE_MASTER;
		nip->meta.pfs_subtype = HAMMER2_PFSSUBTYPE_SNAPSHOT;
		nip->meta.op_flags |= HAMMER2_OPFLAG_PFSROOT;
		nchain->bref.embed.stats = chain->bref.embed.stats;

		kern_uuidgen(&nip->meta.pfs_fsid, 1);

#if 0
		/*
		 * Give the snapshot its own private cluster id.  As a
		 * snapshot no further synchronization with the original
		 * cluster will be done.
		 */
		if (chain->flags & HAMMER2_CHAIN_PFSBOUNDARY)
			nip->meta.pfs_clid = opfs_clid;
		else
			kern_uuidgen(&nip->meta.pfs_clid, 1);
#endif
		kern_uuidgen(&nip->meta.pfs_clid, 1);
		nchain->bref.flags |= HAMMER2_BREF_FLAG_PFSROOT;

		/* XXX hack blockset copy */
		/* XXX doesn't work with real cluster */
		wipdata->meta = nip->meta;
		wipdata->u.blockset = ripdata->u.blockset;

		KKASSERT(wipdata == &nchain->data->ipdata);

		hammer2_chain_unlock(nchain);
		hammer2_inode_ref(nip);
		hammer2_inode_unlock(nip);
		hammer2_inode_chain_sync(nip);
		hammer2_inode_chain_flush(nip);
		KKASSERT(nip->refs == 1);
		hammer2_inode_drop(nip);

		force_local = (hmp->hflags & HMNT2_LOCAL) ? hmp : NULL;

		hammer2_chain_lock(nchain, HAMMER2_RESOLVE_ALWAYS);
		wipdata = &nchain->data->ipdata;
		kprintf("SNAPSHOT LOCAL PFS (IOCTL): %s\n", wipdata->filename);
		hammer2_pfsalloc(nchain, wipdata, nchain->bref.modify_tid,
				 force_local);
		nchain->pmp->inode_tid = starting_inum;

		hammer2_chain_unlock(nchain);
		hammer2_chain_drop(nchain);
	}

	hammer2_chain_unlock(chain);
	hammer2_chain_drop(chain);

	hammer2_inode_unlock(ip);
	hammer2_trans_done(pmp, 1);

	lockmgr(&hmp->bulklk, LK_RELEASE);

	return (hammer2_error_to_errno(error));
}

/*
 * Retrieve the raw inode structure, non-inclusive of node-specific data.
 */
static int
hammer2_ioctl_inode_get(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_inode_t *ino;
	hammer2_chain_t *chain;
	int error;
	int i;

	ino = data;
	error = 0;

	hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);
	ino->data_count = 0;
	ino->inode_count = 0;
	for (i = 0; i < ip->cluster.nchains; ++i) {
		if ((chain = ip->cluster.array[i].chain) != NULL) {
			if (ino->data_count <
			    chain->bref.embed.stats.data_count) {
				ino->data_count =
					chain->bref.embed.stats.data_count;
			}
			if (ino->inode_count <
			    chain->bref.embed.stats.inode_count) {
				ino->inode_count =
					chain->bref.embed.stats.inode_count;
			}
		}
	}
	bzero(&ino->ip_data, sizeof(ino->ip_data));
	ino->ip_data.meta = ip->meta;
	ino->kdata = ip;
	hammer2_inode_unlock(ip);

	return hammer2_error_to_errno(error);
}

/*
 * Set various parameters in an inode which cannot be set through
 * normal filesystem VNOPS.
 */
static int
hammer2_ioctl_inode_set(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_inode_t *ino = data;
	int error = 0;

	hammer2_trans_init(ip->pmp, 0);
	hammer2_inode_lock(ip, 0);

	if ((ino->flags & HAMMER2IOC_INODE_FLAG_CHECK) &&
	    ip->meta.check_algo != ino->ip_data.meta.check_algo) {
		hammer2_inode_modify(ip);
		ip->meta.check_algo = ino->ip_data.meta.check_algo;
	}
	if ((ino->flags & HAMMER2IOC_INODE_FLAG_COMP) &&
	    ip->meta.comp_algo != ino->ip_data.meta.comp_algo) {
		hammer2_inode_modify(ip);
		ip->meta.comp_algo = ino->ip_data.meta.comp_algo;
	}
	ino->kdata = ip;
	
	/* Ignore these flags for now...*/
	if ((ino->flags & HAMMER2IOC_INODE_FLAG_IQUOTA) &&
	    ip->meta.inode_quota != ino->ip_data.meta.inode_quota) {
		hammer2_inode_modify(ip);
		ip->meta.inode_quota = ino->ip_data.meta.inode_quota;
	}
	if ((ino->flags & HAMMER2IOC_INODE_FLAG_DQUOTA) &&
	    ip->meta.data_quota != ino->ip_data.meta.data_quota) {
		hammer2_inode_modify(ip);
		ip->meta.data_quota = ino->ip_data.meta.data_quota;
	}
	if ((ino->flags & HAMMER2IOC_INODE_FLAG_COPIES) &&
	    ip->meta.ncopies != ino->ip_data.meta.ncopies) {
		hammer2_inode_modify(ip);
		ip->meta.ncopies = ino->ip_data.meta.ncopies;
	}
	hammer2_inode_unlock(ip);
	hammer2_trans_done(ip->pmp, 1);

	return (hammer2_error_to_errno(error));
}

static
int
hammer2_ioctl_debug_dump(hammer2_inode_t *ip, u_int flags)
{
	hammer2_chain_t *chain;
	int count = 100000;
	int i;

	for (i = 0; i < ip->cluster.nchains; ++i) {
		chain = ip->cluster.array[i].chain;
		if (chain == NULL)
			continue;
		hammer2_dump_chain(chain, 0, &count, 'i', flags);
	}
	return 0;
}

/*
 * Executes one flush/free pass per call.  If trying to recover
 * data we just freed up a moment ago it can take up to six passes
 * to fully free the blocks.  Note that passes occur automatically based
 * on free space as the storage fills up, but manual passes may be needed
 * if storage becomes almost completely full.
 */
static
int
hammer2_ioctl_bulkfree_scan(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_bulkfree_t *bfi = data;
	hammer2_dev_t	*hmp;
	hammer2_pfs_t	*pmp;
	hammer2_chain_t *vchain;
	int error;
	int didsnap;

	pmp = ip->pmp;
	ip = pmp->iroot;

	hmp = pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);
	if (bfi == NULL)
		return (EINVAL);

	/*
	 * Bulkfree has to be serialized to guarantee at least one sync
	 * inbetween bulkfrees.
	 */
	error = lockmgr(&hmp->bflock, LK_EXCLUSIVE | LK_PCATCH);
	if (error)
		return error;

	/*
	 * sync the filesystem and obtain a snapshot of the synchronized
	 * hmp volume header.  We treat the snapshot as an independent
	 * entity.
	 *
	 * If ENOSPC occurs we should continue, because bulkfree is the only
	 * way to fix that.  The flush will have flushed everything it could
	 * and not left any modified chains.  Otherwise an error is fatal.
	 */
	error = hammer2_vfs_sync(pmp->mp, MNT_WAIT);
	if (error && error != ENOSPC)
		goto failed;

	/*
	 * If we have an ENOSPC error we have to bulkfree on the live
	 * topology.  Otherwise we can bulkfree on a snapshot.
	 */
	if (error) {
		kprintf("hammer2: WARNING! Bulkfree forced to use live "
			"topology\n");
		vchain = &hmp->vchain;
		hammer2_chain_ref(vchain);
		didsnap = 0;
	} else {
		vchain = hammer2_chain_bulksnap(hmp);
		didsnap = 1;
	}

	/*
	 * Bulkfree on a snapshot does not need a transaction, which allows
	 * it to run concurrently with any operation other than another
	 * bulkfree.
	 *
	 * If we are running bulkfree on the live topology we have to be
	 * in a FLUSH transaction.
	 */
	if (didsnap == 0)
		hammer2_trans_init(pmp, HAMMER2_TRANS_ISFLUSH);

	if (bfi) {
		hammer2_thr_freeze(&hmp->bfthr);
		error = hammer2_bulkfree_pass(hmp, vchain, bfi);
		hammer2_thr_unfreeze(&hmp->bfthr);
	}
	if (didsnap) {
		hammer2_chain_bulkdrop(vchain);
	} else {
		hammer2_chain_drop(vchain);
		hammer2_trans_done(pmp, 1);
	}
	error = hammer2_error_to_errno(error);

failed:
	lockmgr(&hmp->bflock, LK_RELEASE);
	return error;
}

/*
 * Unconditionally delete meta-data in a hammer2 filesystem
 */
static
int
hammer2_ioctl_destroy(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_destroy_t *iocd = data;
	hammer2_pfs_t *pmp = ip->pmp;
	int error;

	if (pmp->ronly) {
		error = EROFS;
		return error;
	}

	switch(iocd->cmd) {
	case HAMMER2_DELETE_FILE:
		/*
		 * Destroy a bad directory entry by name.  Caller must
		 * pass the directory as fd.
		 */
		{
		hammer2_xop_unlink_t *xop;

		if (iocd->path[sizeof(iocd->path)-1]) {
			error = EINVAL;
			break;
		}
		if (ip->meta.type != HAMMER2_OBJTYPE_DIRECTORY) {
			error = EINVAL;
			break;
		}
		hammer2_pfs_memory_wait(ip, 0);
		hammer2_trans_init(pmp, 0);
		hammer2_inode_lock(ip, 0);

		xop = hammer2_xop_alloc(ip, HAMMER2_XOP_MODIFYING);
		hammer2_xop_setname(&xop->head, iocd->path, strlen(iocd->path));
		xop->isdir = -1;
		xop->dopermanent = H2DOPERM_PERMANENT |
				   H2DOPERM_FORCE |
				   H2DOPERM_IGNINO;
		hammer2_xop_start(&xop->head, &hammer2_unlink_desc);

		error = hammer2_xop_collect(&xop->head, 0);
		error = hammer2_error_to_errno(error);
		hammer2_inode_unlock(ip);
		hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
		hammer2_trans_done(pmp, 1);
		}
		break;
	case HAMMER2_DELETE_INUM:
		/*
		 * Destroy a bad inode by inode number.
		 */
		{
		hammer2_xop_lookup_t *xop;

		if (iocd->inum < 1) {
			error = EINVAL;
			break;
		}
		hammer2_pfs_memory_wait(ip, 0);
		hammer2_trans_init(pmp, 0);

		xop = hammer2_xop_alloc(pmp->iroot, HAMMER2_XOP_MODIFYING);
		xop->lhc = iocd->inum;
		hammer2_xop_start(&xop->head, &hammer2_delete_desc);
		error = hammer2_xop_collect(&xop->head, 0);
		error = hammer2_error_to_errno(error);
		hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
		hammer2_trans_done(pmp, 1);
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}
