/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/shm.h>

#include "gtm_ipc.h"
#include "gtm_limits.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stat.h"

#include "add_inter.h"
#include "interlock.h"
#include "gtmio.h"
#include <rtnhdr.h>
#include "relinkctl.h"
#include "mmrhash.h"
#include "iosp.h"
#include "do_shmat.h"
#include "hashtab.h"
#include "ipcrmid.h"
#include "eintr_wrappers.h"
#include "stack_frame.h"
#ifdef DEBUG
#  include "wbox_test_init.h"
#  include "toktyp.h"		/* needed by "valid_mname.h" */
#  include "valid_mname.h"
#endif
#include "arlinkdbg.h"
#include "min_max.h"
#include "rtnobj.h"
#include "gtmmsg.h"
#include "hugetlbfs_overrides.h"	/* for the ADJUST_SHM_SIZE_FOR_HUGEPAGES macro */
#include "gtm_permissions.h"

/* This module contains routines that maintain autorelink 'relinkctl' structures.
 * TODO - add description
 */

DEBUG_ONLY(GBLDEF int	saved_errno;)
GBLREF	uint4		process_id;
GBLREF	rtn_tabent	*rtn_names, *rtn_names_end;
GBLREF	stack_frame	*frame_pointer;
OS_PAGE_SIZE_DECLARE

STATICFNDCL void relinkctl_map(open_relinkctl_sgm *linkctl);
STATICFNDCL void relinkctl_unmap(open_relinkctl_sgm *linkctl);
STATICFNDCL int relinkctl_fcntl_lock(int fd, int l_type);
STATICFNDCL void relinkctl_delete(open_relinkctl_sgm *linkctl);

error_def(ERR_PERMGENFAIL);
error_def(ERR_RELINKCTLERR);
error_def(ERR_RELINKCTLFULL);
error_def(ERR_REQRLNKCTLRNDWN);
error_def(ERR_RLNKCTLRNDWNFL);
error_def(ERR_RLNKCTLRNDWNSUC);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/* Routine called to see if a relinkctl structure already exists for the given zroutines element.
 *
 * Parameters:
 *   - obj_container_name - object container name string
 *
 * Output:
 *   - Found or newly created private structure which points to shared relink control structure
 */
open_relinkctl_sgm *relinkctl_attach(mstr *obj_container_name)
{
	open_relinkctl_sgm 	*linkctl, *new_link;
	int			i, len, save_errno;
	mstr			objdir;
	char			pathin[GTM_PATH_MAX], resolvedpath[GTM_PATH_MAX];	/* Includes null terminator char */
	char			*pathptr;
	boolean_t		pathfound;
	char			relinkctl_path[GTM_PATH_MAX], *ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef AUTORELINK_SUPPORTED
	/* Directory name normalization - the directory name must always be the same for purposes of mapping the relinkctl
	 * file. To accomplish this takes two steps:
	 *    1. Use realpath() on the directory name to disambiguate it.
	 *    2. Remove trailing slash(es) so the object directory name.
	 */
	pathfound = TRUE;				/* Assume we'll find the path */
	assert(GTM_PATH_MAX > obj_container_name->len);	/* Should have been checked by our caller */
	memcpy(pathin, obj_container_name->addr, obj_container_name->len);
	pathin[obj_container_name->len] = '\0';		/* Needs null termination for realpath call */
	pathptr = realpath(pathin, resolvedpath);
	if (NULL == pathptr)
	{
		if (ENOENT == (save_errno = errno))	/* Note assignment */
		{
			pathfound = FALSE;		/* Path no longer exists - use our best attempt to find it */
			pathptr = pathin;
		} else
			/* This error is appropriate here as the directory was checked in the caller before coming here
			 * so this error is "just-in-case".
			 */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("realpath()"), CALLFROM,
				      save_errno);
	}
	objdir.addr = pathptr;
	objdir.len = strlen(pathptr);
	assert(objdir.len < ARRAYSIZE(resolvedpath));
	while ((1 < objdir.len) && ('/' == *(objdir.addr + objdir.len - 1)))
		objdir.len--;
	/* Now look the directory up in our list to see if we have it or not already*/
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
	{
		if (MSTR_EQ(&objdir, &linkctl->zro_entry_name))
			return linkctl;
	}
	/* If realpath() didn't find the path and we don't already have it open, don't create a relinkctl file for it. Note if
	 * the caller is op_zrupdate(), very shortly we are going to try to use the errno value set above in realpath(). If
	 * anything gets added to this module between the realpath() above and here, a different mechanism for passing errno
	 * needs to be found.
	 */
	if (!pathfound)
		return NULL;
	/* Not already open */
	len = relinkctl_get_key(relinkctl_path, &objdir) + 1;	/* + 1 for trailing null in relinkctl_path */
	assert(len <= ARRAYSIZE(relinkctl_path));
	new_link = malloc(SIZEOF(open_relinkctl_sgm) + objdir.len + 1 + len);	/* + 1 for trailing null in zro_entry_name */
	memset(new_link, 0, SIZEOF(open_relinkctl_sgm));
	new_link->zro_entry_name.len = objdir.len;
	new_link->zro_entry_name.addr = ptr = (char *)(new_link + 1);
	memcpy(ptr, objdir.addr, objdir.len);
	ptr += objdir.len;
	*ptr++ = '\0';	/* trailing null for "new_link->zro_entry_name" */
	new_link->relinkctl_path = ptr;
	memcpy(ptr, relinkctl_path, len);
	assert('\0' == new_link->relinkctl_path[len - 1]);
	for (i = 0; i < NUM_RTNOBJ_SHM_INDEX; i++)
		new_link->rtnobj_shmid[i] = INVALID_SHMID;
	/* Open + map structure */
	relinkctl_open(new_link);	/* initializes new_link->fd, new_link->hdr, new_link->n_records,
					 * new_link->rec_base, new_link->shm_hashbase and new_link->locked
					 */
	/* Add to open list */
	new_link->next = TREF(open_relinkctl_list);
	TREF(open_relinkctl_list) = new_link;
	return new_link;
#	else
	return NULL;
#	endif
}

/* Routine to open and mmap a relinkctl file for a given $ZROUTINES object directory.
 *
 * Parameter:
 *    linkctl - open_relinkctl_sgm (process private) block describing shared (mmap'd) entry and supplying the path/name
 *              of the relinkctl file comprised of both static and hashed directory name values for uniqueness.
 *
 * Returns:
 *    Various fields in linkctl (fd, hdr).
 *
 * The control structure should be both readable *and* writable by anything that can read the object directory.
 */
void relinkctl_open(open_relinkctl_sgm *linkctl)
{
#	ifdef AUTORELINK_SUPPORTED
	int			fd, i, j, rc, save_errno, shmid, status, retries;
	struct stat     	stat_buf;
	size_t			shm_size;
	boolean_t		is_mu_rndwn_rlnkctl, shm_removed;
	relinkshm_hdr_t		*shm_base;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	relinkctl_data		*hdr;
	char			errstr[256];
	struct stat		dir_stat_buf;
	int			stat_res;
	int			user_id;
	int			group_id;
	int			perm;
	struct perm_diag_data	pdd;
	struct shmid_ds		shmstat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	linkctl->hdr = NULL;
	/* open the given relinkctl file */
	DBGARLNK((stderr, "relinkctl_open: pid = %d : Opening relinkctl file %s for entry %.*s\n", getpid(),
		linkctl->relinkctl_path, linkctl->zro_entry_name.len, linkctl->zro_entry_name.addr));
	/* Anybody that has read permissions to the object container should have write permissions to the relinkctl file */
	retries = 0;
	is_mu_rndwn_rlnkctl = TREF(is_mu_rndwn_rlnkctl);
	do
	{
		assert('\0' == linkctl->zro_entry_name.addr[linkctl->zro_entry_name.len]);	/* needed for STAT_FILE */
		STAT_FILE(linkctl->zro_entry_name.addr, &dir_stat_buf, stat_res);
		if (-1 == stat_res)
		{
			SNPRINTF(errstr, SIZEOF(errstr), "stat() of file %s failed", linkctl->zro_entry_name.addr);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, errno);
		}
		if (gtm_permissions(&dir_stat_buf, &user_id, &group_id, &perm, PERM_IPC, &pdd) < 0)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(&linkctl->zro_entry_name),
					ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("relinkctl"),
					RTS_ERROR_MSTR(&linkctl->zro_entry_name));
		}
		/* Attempt to create file with desired permissions */
		fd = OPEN3(linkctl->relinkctl_path, O_CREAT | O_RDWR | O_EXCL, perm);
		/* If file created, set owner/group, if needed */
		if ((FD_INVALID != fd)
			&& (((-1 != user_id) && (user_id != stat_buf.st_uid))
				|| ((-1 != group_id) && (group_id != stat_buf.st_gid)))
			&& (-1 == fchown(fd, user_id, group_id)))
		{
			save_errno = errno;
			CLOSEFILE_RESET(fd, rc);
			SNPRINTF(errstr, SIZEOF(errstr), "fchown() of file %s failed", linkctl->relinkctl_path);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
		}
		if ((FD_INVALID == fd) && (errno == EEXIST))
		{	/* file already existed, open existing */
			fd = OPEN3(linkctl->relinkctl_path, O_CREAT | O_RDWR, 0);
		}
		if (FD_INVALID == fd)
		{
			SNPRINTF(errstr, SIZEOF(errstr), "open() of file %s failed", linkctl->relinkctl_path);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, errno);
		}

		linkctl->fd = fd;
		relinkctl_lock_exclu(linkctl);

		FSTAT_FILE(fd, &stat_buf, status);
		if (0 != status)
		{
			save_errno = errno;
			CLOSEFILE_RESET(fd, rc);
			SNPRINTF(errstr, SIZEOF(errstr), "fstat() of file %s failed", linkctl->relinkctl_path);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
		}
		if (RELINKCTL_MMAP_SZ != stat_buf.st_size)
		{
			DBGARLNK((stderr, "relinkctl_open: file size = %d\n", stat_buf.st_size));
			if (RELINKCTL_MMAP_SZ != stat_buf.st_size)
			{
				FTRUNCATE(fd, RELINKCTL_MMAP_SZ, rc);
				if (0 != rc)
				{
					save_errno = errno;
					relinkctl_unlock_exclu(linkctl);
					SNPRINTF(errstr, SIZEOF(errstr), "ftruncate() of file %s failed", linkctl->relinkctl_path);
					ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
				}
			}
		}
		relinkctl_map(linkctl);	/* linkctl->hdr is now accessible */
		hdr = linkctl->hdr;
		if (hdr->file_deleted)
		{	/* Some other process concurrently opened and closed/deleted the relinkctl file while
			 * we were still inside relinkctl_open. Reattach to new file (create it if necessary)
			 * instead of using the current fd as this wont be visible to other new processes.
			 */
			DBGARLNK((stderr, "relinkctl_open: file_deleted = 1. Retrying open.\n"));
			relinkctl_unlock_exclu(linkctl);
			relinkctl_unmap(linkctl);
			assert(NULL == linkctl->hdr);
			assertpro(16 > retries++);	/* too many loops is not practically possible. prevent infinite loops */
			continue;
		}
		shm_size = RELINKCTL_SHM_SIZE;
		ADJUST_SHM_SIZE_FOR_HUGEPAGES(shm_size, shm_size); /* second parameter "shm_size" is adjusted size */
		if (hdr->initialized)
		{	/* Not creator of shared memory. Need to attach to shared memory.
			 * Need lock (already obtained) to prevent interaction with concurrent initialization, nattached = 0.
			 */
			shmid = hdr->relinkctl_shmid;
			DBGARLNK((stderr, "relinkctl_open: file already initialized : pre-increment hdr->nattached = %d"
				" : shmid = %d\n", hdr->nattached, shmid));
			assert(INVALID_SHMID != shmid);
			assert(!hdr->file_deleted);
			hdr->nattached++;
			if (!is_mu_rndwn_rlnkctl)
				relinkctl_unlock_exclu(linkctl);
			if (-1 == (sm_long_t)(shm_base = (relinkshm_hdr_t *)do_shmat(shmid, 0, 0)))
			{
				save_errno = errno;
				shm_removed = SHM_REMOVED(save_errno);
				/* If shm has been removed, then direct one to use MUPIP RUNDOWN -RELINKCTL */
				if (!shm_removed || !is_mu_rndwn_rlnkctl)
				{
					if (!is_mu_rndwn_rlnkctl)
						relinkctl_lock_exclu(linkctl);
					hdr->nattached--;
					relinkctl_unlock_exclu(linkctl);
					relinkctl_unmap(linkctl);
					if (!shm_removed)
					{
						SNPRINTF(errstr, SIZEOF(errstr), "shmat() failed for "
							"shmid=%d shmsize=%llu [0x%llx]", shmid, shm_size, shm_size);
						ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
					} else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REQRLNKCTLRNDWN, 2,
									RTS_ERROR_MSTR(&linkctl->zro_entry_name));
				} else
				{	/* This is MUPIP RUNDOWN -RELINKCTL and shm is removed. Run this relinkctl file down. */
					DBGARLNK((stderr, "relinkctl_open: Set hdr->initialized to FALSE\n"));
					hdr->initialized = FALSE;
				}
			}
		}
		if (!hdr->initialized)
		{
			DBGARLNK((stderr, "relinkctl_open: file first open\n"));
			hdr->n_records = 0;
			/* Create shared memory to store hash buckets of routine names for faster search in relinkctl file */
			assert(RELINKCTL_HASH_BUCKETS == getprime(RELINKCTL_MAX_ENTRIES));
			shmid = shmget(IPC_PRIVATE, shm_size, RWDALL | IPC_CREAT);
			if (-1 == shmid)
			{
				save_errno = errno;
				relinkctl_delete(linkctl);
				relinkctl_unlock_exclu(linkctl);
				relinkctl_unmap(linkctl);
				SNPRINTF(errstr, SIZEOF(errstr), "shmget() failed for shmsize=%llu [0x%llx]", shm_size, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			if (-1 == shmctl(shmid, IPC_STAT, &shmstat))
			{
				save_errno = errno;
				relinkctl_delete(linkctl);
				relinkctl_unlock_exclu(linkctl);
				relinkctl_unmap(linkctl);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "shmctl(IPC_STAT) failed for shmid=%d shmsize=%llu [0x%llx]",
					shmid, shm_size, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			/* change group and permissions */
			if ((-1 != user_id) && (user_id != shmstat.shm_perm.uid))
				shmstat.shm_perm.uid = user_id;
			if ((-1 != group_id) && (group_id != shmstat.shm_perm.gid))
				shmstat.shm_perm.gid = group_id;
			shmstat.shm_perm.mode = perm;
			if (-1 == shmctl(shmid, IPC_SET, &shmstat))
			{
				save_errno = errno;
				relinkctl_delete(linkctl);
				relinkctl_unlock_exclu(linkctl);
				relinkctl_unmap(linkctl);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "shmctl(IPC_SET) failed for shmid=%d shmsize=%llu [0x%llx]",
					shmid, shm_size, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			/* Initialize shared memory header */
			if (-1 == (sm_long_t)(shm_base = (relinkshm_hdr_t *)do_shmat(shmid, 0, 0)))
			{
				save_errno = errno;
				relinkctl_delete(linkctl);
				relinkctl_unlock_exclu(linkctl);
				relinkctl_unmap(linkctl);
				shm_rmid(shmid);	/* if error removing shmid we created, just move on */
				SNPRINTF(errstr, SIZEOF(errstr), "shmat() failed for shmid=%d shmsize=%llu [0x%llx]",
					shmid, shm_size, shm_size);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			hdr->relinkctl_shmid = shmid;
			hdr->relinkctl_shmlen = shm_size;
			assert(ARRAYSIZE(shm_base->relinkctl_fname) > strlen(linkctl->relinkctl_path));
			assert(0 == ((UINTPTR_T)shm_base % 8));
			assert(0 == (SIZEOF(relinkshm_hdr_t) % SIZEOF(uint4)));	/* assert SIZEOF(*sm_uint_ptr_t) alignment */
			memset(shm_base, 0, shm_size);
			strcpy(shm_base->relinkctl_fname, linkctl->relinkctl_path);
			shm_base->min_shm_index = TREF(relinkctl_shm_min_index);
			shm_base->rtnobj_min_shm_index = NUM_RTNOBJ_SHM_INDEX;
			shm_base->rtnobj_max_shm_index = 0;
			shm_base->rtnobj_shmid_cycle = 0;
			shm_base->rndwn_adjusted_nattch = FALSE;
			DEBUG_ONLY(shm_base->skip_rundown_check = FALSE;)
			for (i = 0; i < NUM_RTNOBJ_SHM_INDEX; i++)
			{
				rtnobj_shm_hdr = &shm_base->rtnobj_shmhdr[i];
				rtnobj_shm_hdr->rtnobj_shmid = INVALID_SHMID;
				rtnobj_shm_hdr->rtnobj_min_free_index = NUM_RTNOBJ_SIZE_BITS;
				for (j = 0; j < NUM_RTNOBJ_SIZE_BITS; j++)
				{
					rtnobj_shm_hdr->freeList[j].fl = NULL_RTNOBJ_SM_OFF_T;
					rtnobj_shm_hdr->freeList[j].bl = NULL_RTNOBJ_SM_OFF_T;
				}
			}
			SET_LATCH_GLOBAL(&shm_base->relinkctl_latch, LOCK_AVAILABLE);
			hdr->nattached = 1;
			hdr->zro_entry_name_len = MIN(linkctl->zro_entry_name.len, ARRAYSIZE(hdr->zro_entry_name) - 1);
			memcpy(hdr->zro_entry_name, linkctl->zro_entry_name.addr, hdr->zro_entry_name_len);
			hdr->zro_entry_name[hdr->zro_entry_name_len] = '\0';
			/* shared memory initialization complete */
			hdr->initialized = TRUE;
			relinkctl_unlock_exclu(linkctl);
		} else if (is_mu_rndwn_rlnkctl)
			relinkctl_unlock_exclu(linkctl);
		assert(!linkctl->locked);
		assert(0 == ((UINTPTR_T)shm_base % 8));
		assert(0 == (SIZEOF(relinkshm_hdr_t) % SIZEOF(uint4)));	/* assert SIZEOF(*sm_uint_ptr_t) alignment */
#		ifdef DEBUG
		if (TREF(gtm_autorelink_keeprtn))
			shm_base->skip_rundown_check = TRUE;
#		endif
		linkctl->shm_hashbase = (sm_uint_ptr_t)(sm_uc_ptr_t)(shm_base + 1);
			/* skip past shm header to reach hash array start */
		assert(0 == ((UINTPTR_T)linkctl->shm_hashbase % 8));	/* assert each section is 8-byte aligned at least */
		assert(1 == RELINKCTL_HASH_BUCKETS % 2); /* RELINKSHM_RTNHASH_SIZE definition relies on this for 8-byte alignment */
		linkctl->rec_base = (relinkrec_t *)((sm_uc_ptr_t)linkctl->shm_hashbase + RELINKSHM_RTNHASH_SIZE);
		assert(128 >= SIZEOF(relinkrec_t));	/* or else adjust CACHELINE_PAD_COND filler downwards */
		assert(0 == ((UINTPTR_T)linkctl->rec_base % 8));	/* assert each section is 8-byte aligned at least */
		break;
	} while (TRUE);
#	endif
}

#ifdef AUTORELINK_SUPPORTED
/* This is called from processes that already have "linkctl" attached but have not done an increment of "linkctl->hdr->nattached".
 * Example is a jobbed off process in ojstartchild.c
 */
void relinkctl_incr_nattached(void)
{
	open_relinkctl_sgm *linkctl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
	{
		relinkctl_lock_exclu(linkctl);
		DBGARLNK((stderr, "relinkctl_incr_nattached : pid = %d : file %s : pre-incr hdr->nattached = %d\n",
			  getpid(), linkctl->relinkctl_path, linkctl->hdr->nattached));
		assert(linkctl->hdr->nattached);
		linkctl->hdr->nattached++;
		relinkctl_unlock_exclu(linkctl);
	}
}

/* Routine to generate unique key for a $ZROUTINES entry name used to create relinkctl file for that entry in the directory
 * $gtm_linktmpdir (e.g. /testarea1/gtm/temp --> $gtm_linktmpdir/gtm-relinkctl-d0f3d074c724430bc1c7679141b96411).
 * Theoretically, we'd need a scheme to resolve hash collisions. Say, append -<collision_id> to the key.
 * But since this is 128-bit MurmurHash3, we can assume a collision will never happen in practice, so we do not
 * handle the extremely unlikely event of a hash collision for the few $ZROUTINES entries used by processes using
 * the same $gtm_linktmpdir value.
 *
 * Parameters:
 *
 *   key            - Generated as $gtm_linktmpdir/gtm-relinkctl-<hash>. Buffer should be GTM_PATH_MAX bytes (output).
 *   zro_entry_name - Address of mstr containing the fully expanded zroutines entry directory name.
 */
int relinkctl_get_key(char key[GTM_PATH_MAX], mstr *zro_entry_name)
{
	gtm_uint16	hash;
	unsigned char	hexstr[33];
	int		keylen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gtmmrhash_128(zro_entry_name->addr, zro_entry_name->len, 0, &hash);
	gtmmrhash_128_hex(&hash, hexstr);
	hexstr[32] = '\0';
	/* TODO: Improve efficiency - use memcpy to build string and provide better max-length checking than an assert */
	keylen = SNPRINTF(&key[0], GTM_PATH_MAX - 1, "%.*s/gtm-relinkctl-%s", (TREF(gtm_linktmpdir)).len,
			  (TREF(gtm_linktmpdir)).addr, hexstr);
	assert((0 < keylen) && (GTM_PATH_MAX > keylen));
	return keylen;
}

/**
 * Relinkctl mmap-related methods
 */

/* Routine to map at least n_records, currently known number of entries.
 *
 * Parameter:
 *      linkctl - addr of process-private block describing the shared file.
 *
 * Fills in:
 * 	linkctl->hdr
 * 	linkctl->n_records
 */
STATICFNDEF void relinkctl_map(open_relinkctl_sgm *linkctl)
{
	sm_uc_ptr_t	addr;

	assert(NULL == linkctl->hdr);
	addr = (sm_uc_ptr_t)mmap(NULL, RELINKCTL_MMAP_SZ, (PROT_READ + PROT_WRITE), MAP_SHARED, linkctl->fd, 0);
	if (MAP_FAILED == addr)
		ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, "mmap()", errno);
	linkctl->hdr = (relinkctl_data *)addr;
	linkctl->n_records = 0;
}

/* Routine similar to relink_map except UNMAPs the file */
STATICFNDEF void relinkctl_unmap(open_relinkctl_sgm *linkctl)
{
	sm_uc_ptr_t	addr;
	int		rc;

	addr = (sm_uc_ptr_t)linkctl->hdr;
	munmap(addr, RELINKCTL_MMAP_SZ); /* If munmap errors, it seems better to move on than stop for a non-critical error */
	linkctl->hdr = NULL;
	linkctl->n_records = 0;
	CLOSEFILE_RESET(linkctl->fd, rc);
}
#endif

/**
 * Exclusive locking methods controlling WRITE access to relinkctl control files.
 */

/* Routine to exclusively lock the relinkctl file.
 *
 * Parameter:
 *
 *   linkctl - address of relink control structure for a given $ZROUTINEs node
 */
void relinkctl_lock_exclu(open_relinkctl_sgm *linkctl)
{
	int	status;

#	ifdef AUTORELINK_SUPPORTED
	assert(!linkctl->locked);
	if (linkctl->locked)
		return;
	status = relinkctl_fcntl_lock(linkctl->fd, F_WRLCK);
	if (-1 == status)
		ISSUE_RELINKCTLERR_TEXT(&linkctl->zro_entry_name, "fcntl() lock attempt failed", errno);
	linkctl->locked = TRUE;
#	endif
	return;
}

/* Routine same as relinkctl_lock_exclu() but instead UNLOCKs the lock */
void relinkctl_unlock_exclu(open_relinkctl_sgm *linkctl)
{
	int	status;

#	ifdef AUTORELINK_SUPPORTED
	assert(linkctl->locked);
	if (!linkctl->locked)
		return;
	status = relinkctl_fcntl_lock(linkctl->fd, F_UNLCK);
	if (-1 == status)
		ISSUE_RELINKCTLERR_TEXT(&linkctl->zro_entry_name, "fcntl() unlock attempt failed", errno);
	linkctl->locked = FALSE;
#	endif
	return;
}

/* Routine to set or remove an advisory lock on a given relinkctl file.
 *
 * Parameters:
 *
 *   fd	    - File descriptor of file to lock.
 *   l_type - Lock type (F_WRLCK for lock or F_UNLCK for unlock)
 *
 * Return value:
 *
 *   - status of fcntl() lock or unlock attempt
 */
#ifdef AUTORELINK_SUPPORTED
STATICFNDEF int relinkctl_fcntl_lock(int fd, int l_type)
{
	int		status;
	struct flock	lock;

	assert((F_WRLCK == l_type) || (F_UNLCK == l_type));
	do
	{
		lock.l_type = l_type;
		lock.l_whence = SEEK_SET;       /* Locking offsets from file beginning */
		lock.l_start = lock.l_len = 0;  /* Lock the whole file */
		lock.l_pid = process_id;
	} while ((-1 == (status = fcntl(fd, F_SETLKW, &lock))) && (EINTR == errno));
	return status;
}
#endif

#ifdef AUTORELINK_SUPPORTED
/**
 * Relinkctl file record management routines
 */

/* Like relinkctl_find_record, but inserts a new entry instead of returning NULL */
relinkrec_t *relinkctl_insert_record(open_relinkctl_sgm *linkctl, mstr *rtnname)
{
	relinkrec_t	*base, *rec;
	uint4		hash, prev_hash_index, nrec;

	COMPUTE_RELINKCTL_HASH(rtnname, hash);
	rec = relinkctl_find_record(linkctl, rtnname, hash, &prev_hash_index);
	if (NULL == rec)
	{	/* Record not found while not under lock - lock it and try again */
		relinkctl_lock_exclu(linkctl);
		rec = relinkctl_find_record(linkctl, rtnname, hash, &prev_hash_index);
		if (NULL == rec)
		{	/* Add a new record if room exists - else error */
			assert(linkctl->locked);
			nrec = linkctl->n_records;
			assert(nrec == linkctl->hdr->n_records);	/* Assured by relinkctl_find_record() */
			if (RELINKCTL_MAX_ENTRIES == linkctl->hdr->n_records)
			{
				relinkctl_unlock_exclu(linkctl);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RELINKCTLFULL, 3, linkctl->zro_entry_name.len,
					      linkctl->zro_entry_name.addr, RELINKCTL_MAX_ENTRIES);
			}
			base = linkctl->rec_base;
			rec = base + nrec;
			memset(&rec->rtnname_fixed.c[0], 0, SIZEOF(mident_fixed));
			assert(MAX_MIDENT_LEN >= rtnname->len);
			/* assert(valid_mname(rtnname)) is not needed here because it was already done in relinkctl_find_record */
			memcpy(&rec->rtnname_fixed.c[0], rtnname->addr, rtnname->len);
			rec->cycle = 0;	/* Note incr_link() will bump this to 1 when routine is linked */
			rec->hashindex_fl = 0;
			rec->objhash = 0;
			rec->rtnobj_shm_offset = (rtnobj_sm_off_t)NULL_RTNOBJ_SM_OFF_T;
			if (prev_hash_index)
			{
				--prev_hash_index;
				assert(prev_hash_index < nrec);
				assert(0 == base[prev_hash_index].hashindex_fl);
				base[prev_hash_index].hashindex_fl = nrec + 1;
			} else
				linkctl->shm_hashbase[hash] = nrec + 1;
			linkctl->hdr->n_records++;
			assert(rec == relinkctl_find_record(linkctl, rtnname, hash, &prev_hash_index));
		}
		relinkctl_unlock_exclu(linkctl);
	}
	return rec;
}

/* Iterate through each relink_record_struct starting at (relink_record_ptr)&linkctl->map_addr[0]
 * Find rec s.t. rec->rtnname == rtnname, return offset of rec.
 * Otherwise, return NOMATCH (defined 0xffff..).
 */
relinkrec_t *relinkctl_find_record(open_relinkctl_sgm *linkctl, mstr *rtnname, uint4 hash, uint4 *prev_hash_index)
{
	relinkrec_t		*rec, *base;
	unsigned int		nrec, index;
	sm_uint_ptr_t		ptr;
	uint4			rtnhash, prev_index;

	assert(valid_mname(rtnname));
	linkctl->n_records = linkctl->hdr->n_records;	/* Make sure we search among all currently existing records */
	ptr = linkctl->shm_hashbase;
	assert(RELINKCTL_HASH_BUCKETS > hash);
	ptr += hash;
	prev_index = *ptr;
	index = prev_index - 1;	/* 'index' is unsigned so will become huge positive number in case *ptr is 0 */
	base = linkctl->rec_base;
	nrec = linkctl->n_records;
	assert(RELINKCTL_MAX_ENTRIES >= nrec);
	while (index < nrec)
	{
		rec = &base[index];
		DEBUG_ONLY(COMPUTE_RELINKCTL_HASH(rtnname, rtnhash);)
		assert(rtnhash == hash);
		/* Check routine name plus null trailer in fixed version */
		if ((0 == memcmp(&rec->rtnname_fixed.c, rtnname->addr, rtnname->len))
				&& ('\0' == rec->rtnname_fixed.c[rtnname->len]))
			return rec;
		assert(RELINKCTL_MAX_ENTRIES >= rec->hashindex_fl);
		prev_index = index + 1;
		index = rec->hashindex_fl - 1;
	}
	*prev_hash_index = prev_index;
	return NULL;
}
#endif

/**
 * Relinkctl file rundown routines
 */

/*
 * Unmap all relinkctl structs after (atomically) decrementing nattached field in relinkctl file header
 */
void relinkctl_rundown(boolean_t decr_attached, boolean_t do_rtnobj_shm_free)
{
#	ifdef AUTORELINK_SUPPORTED
	int			rc;
	open_relinkctl_sgm	*linkctl, *nextctl;
	rtn_tabent		*rtab;
	rhdtyp			*rtnhdr;
	relinkctl_data		*hdr;
	int			shmid, i, nattached;
	relinkshm_hdr_t		*shm_hdr;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	stack_frame		*fp;
	boolean_t		is_mu_rndwn_rlnkctl, remove_shm;
	struct shmid_ds		shm_buf;
#	ifdef DEBUG
	relinkrec_t		*linkrec, *linktop;
	int			j, k;
	rtnobj_sm_off_t		rtnobj_shm_offset;
	boolean_t		clean_shutdown;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (do_rtnobj_shm_free && !TREF(gtm_autorelink_keeprtn))
	{
		assert(process_exiting);
		/* Run through all loaded routines and if any are loaded in shared memory, decrement their reference counts.
		 * Since this involves shared memory, this is best accomplished by invoking rtnobj_shm_free.
		 * For non-shared objects, zr_unlink_rtn will only release process-private memory. Since this process is
		 * going to die (asserted by process_exiting above) no need to do anything in that case.
		 * But before that check if the M-stack has any frames with rtnhdr->rtn_relinked TRUE. These are
		 * routine-header structures that are not added to the rtn_names[] array but could potentially have
		 * done rtnobj_shm_malloc(). In that case do rtnobj_shm_free() on those to decrement shared memory refcnts.
		 */
		for (fp = frame_pointer; NULL != fp; fp = SKIP_BASE_FRAME(fp->old_frame_pointer))
		{
			rtnhdr = CURRENT_RHEAD_ADR(fp->rvector);
			if ((NULL != rtnhdr) && rtnhdr->rtn_relinked && rtnhdr->shared_object)
			{
				rtnobj_shm_free(rtnhdr, LATCH_GRABBED_FALSE);
				rtnhdr->rtn_relinked = FALSE;
				rtnhdr->shared_object = FALSE;
			}
		}
		for (rtab = rtn_names_end; rtab > rtn_names; rtab--, rtn_names_end = rtab)
		{	/* [0] is not used (for some reason) */
			rtnhdr = rtab->rt_adr;
			if (rtnhdr->shared_object)
				rtnobj_shm_free(rtnhdr, LATCH_GRABBED_FALSE);
		}
	}
	is_mu_rndwn_rlnkctl = TREF(is_mu_rndwn_rlnkctl);
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = nextctl)
	{
		for (i = 0; i < NUM_RTNOBJ_SHM_INDEX; i++)
		{
			if (NULL != linkctl->rtnobj_shm_base[i])
			{
				assert(INVALID_SHMID != linkctl->rtnobj_shmid[i]);
				SHMDT(linkctl->rtnobj_shm_base[i]);
				linkctl->rtnobj_shm_base[i] = NULL;
			} else
				assert(INVALID_SHMID == linkctl->rtnobj_shmid[i]);
		}
		if (decr_attached)
		{
			relinkctl_lock_exclu(linkctl);
			hdr = linkctl->hdr;
			assert(0 < hdr->nattached);
			if (0 < hdr->nattached)
				hdr->nattached--;
			nattached = hdr->nattached;
			DBGARLNK((stderr, "relinkctl_rundown : pid = %d : file %s : post-decr nattached = %d\n",
				getpid(), linkctl->relinkctl_path, nattached));
			assert(0 <= nattached);
			assert(INVALID_SHMID != hdr->relinkctl_shmid);
			shm_hdr = GET_RELINK_SHM_HDR(linkctl);
			if (0 == nattached)
			{
				DBGARLNK((stderr, "relinkctl_rundown : nattached = 0\n"));
				remove_shm = TRUE;
			} else if (is_mu_rndwn_rlnkctl)
			{	/* If MUPIP RUNDOWN -RELINKCTL, check if shm_buff.nattch is 1 (i.e. mupip rundown -relinkctl
				 * is the only one attached to this shm). If so, ignore nattached and run this shm down.
				 * Most likely processees that had bumped nattached got kill -9ed.
				 * If shm_buff.nattch is not 1, fix hdr->nattached to match shm_buf.nattch so when the time
				 * comes for the last GTM process to rundown, it will remove the shm without the need for
				 * any more MUPIP RUNDOWN -RELINKCTL commands.
				 */
				shmid = hdr->relinkctl_shmid;
				if (0 != shmctl(shmid, IPC_STAT, &shm_buf))
				{
					assert(FALSE);
					remove_shm = FALSE;
				} else
				{
					nattached = shm_buf.shm_nattch - 1; /* remove self since we will do a SHMDT soon */
					if (hdr->nattached != nattached)
					{
						hdr->nattached = nattached;	/* fix hdr->nattached while at this */
						shm_hdr->rndwn_adjusted_nattch = TRUE;
					}
					remove_shm = !nattached;
				}
				if (!remove_shm)
				{
					assert(nattached);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RLNKCTLRNDWNFL, 3,
								RTS_ERROR_MSTR(&linkctl->zro_entry_name), nattached);
				}
			} else
				remove_shm = FALSE;
			if (remove_shm)
			{
#				ifdef DEBUG
				clean_shutdown = (!shm_hdr->rndwn_adjusted_nattch && !shm_hdr->skip_rundown_check);
				if (clean_shutdown)
				{
					/* Check that ALL reference counts are zero */
					for (linkrec = linkctl->rec_base, linktop = &linkrec[hdr->n_records];
						linkrec < linktop; linkrec++)
					{
						assert(grab_latch(&linkrec->rtnobj_latch, 0));	/* 0 => do not wait for latch */
						DEBUG_ONLY(rtnobj_shm_offset = linkrec->rtnobj_shm_offset;)
						assert(NULL_RTNOBJ_SM_OFF_T == rtnobj_shm_offset);
						assert(0 == linkrec->objLen);
						assert(0 == linkrec->usedLen);
					}
					grab_latch(&shm_hdr->relinkctl_latch, 0);	/* 0 => do not wait for latch */
				}
				/* No need to release latch as we will be deleting the shared memory anyways */
#				endif
				for (i = 0; i < NUM_RTNOBJ_SHM_INDEX; i++)
				{
					rtnobj_shm_hdr = &shm_hdr->rtnobj_shmhdr[i];
					assert(!clean_shutdown || (0 == rtnobj_shm_hdr->real_len));
					assert(!clean_shutdown || (0 == rtnobj_shm_hdr->used_len));
					shmid = rtnobj_shm_hdr->rtnobj_shmid;
					if (INVALID_SHMID != shmid)
					{
#						ifdef DEBUG
						if (clean_shutdown)
						{	/* Note: shmctl can fail if some other process changes permissions
							 * concurrently (after we had attached to the shmid) so account for
							 * that in the assert below.
							 */
							if (0 == shmctl(shmid, IPC_STAT, &shm_buf))
							{
							/* We expect no one else to be attached to relinkctl shm. One case we
							 * know of is if a process opens a PIPE device and iopi_open forks off
							 * a child (which would cause shm_nattch to increment implicitly) and
							 * continues execution issuing say a CRYPTNOSEEK error and exit BEFORE the
							 * child process has done the EXEC (which would cause the shm_nattch to
							 * decrement it back to the expected value). Account for it in the assert.
							 */
								assert((0 == shm_buf.shm_nattch) || TREF(fork_without_child_wait));
							}
							k = i + MIN_RTNOBJ_SHM_INDEX - MIN_RTNOBJ_SIZE_BITS;
							assert(rtnobj_shm_hdr->shm_len
								== ((gtm_uint64_t)1 << (k + MIN_RTNOBJ_SIZE_BITS)));
							for (j = 0; j < NUM_RTNOBJ_SIZE_BITS; j++)
							{
								if (j != k)
								{
									assert(NULL_RTNOBJ_SM_OFF_T
										== rtnobj_shm_hdr->freeList[j].fl);
									assert(NULL_RTNOBJ_SM_OFF_T
										== rtnobj_shm_hdr->freeList[j].bl);
								} else
								{
									assert(OFFSETOF(rtnobj_hdr_t, userStorage)
											== rtnobj_shm_hdr->freeList[j].fl);
									assert(OFFSETOF(rtnobj_hdr_t, userStorage)
										== rtnobj_shm_hdr->freeList[j].bl);
								}
							}
						}
#						endif
						shm_rmid(shmid); /* If error removing shmid, not much we can do. Just move on */
					}
				}
				SHMDT(shm_hdr);		/* If error detaching, not much we can do. Just move on */
				shmid = hdr->relinkctl_shmid;
				shm_rmid(shmid);	/* If error removing shmid, not much we can do. Just move on */
				relinkctl_delete(linkctl);
				if (is_mu_rndwn_rlnkctl)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RLNKCTLRNDWNSUC, 2,
								RTS_ERROR_MSTR(&linkctl->zro_entry_name));
			} else
			{
				SHMDT(shm_hdr);		/* If error detaching, not much we can do. Just move on */
			}
			linkctl->rec_base = NULL;
			linkctl->shm_hashbase = NULL;
			relinkctl_unlock_exclu(linkctl);
		}
		relinkctl_unmap(linkctl);	/* sets "linkctl->hdr" to NULL */
		nextctl = linkctl->next;
		free(linkctl);
	}
	TREF(open_relinkctl_list) = NULL;
#	endif
	return;
}

#ifdef AUTORELINK_SUPPORTED
/*
 * Clean up (i.e. delete) relinkctl file whose descriptor block is passed in
 */
STATICFNDEF void relinkctl_delete(open_relinkctl_sgm *linkctl)
{
	int	status;

	DBGARLNK((stderr, "relinkctl_delete : pid = %d : Deleting %s\n", getpid(), linkctl->relinkctl_path));
	status = UNLINK(linkctl->relinkctl_path);
	/* If unlink succeeds, then set "hdr->file_deleted" to TRUE to notify other processes in relinkctl_open
	 * about this concurrent delete so they can reattach. If the unlink fails (e.g. due to permission issues)
	 * do not set "hdr->file_deleted".
	 */
	if (-1 != status)
		linkctl->hdr->file_deleted = TRUE; /* Notify any other process in relinkctl_open about this so they can reattach */
	else
		linkctl->hdr->initialized = FALSE;	/* So next process reinitializes hdr->relinkctl_shmid etc. */
	return;
}
#endif
