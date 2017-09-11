/****************************************************************
 *								*
 * Copyright (c) 2014-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "sleep.h"
#include "time.h"

/* This module contains routines that maintain autorelink 'relinkctl' structures */

/* Constants defining how many times to retry the loop in relinkctl_open() based on the specific error conditions encountered. */
#define MAX_RCTL_INIT_WAIT_RETRIES	1000	/* # of sleeps to allow while waiting for the shared memory to be initialized. */
#define MAX_RCTL_DELETED_RETRIES	16	/* # of times to allow an existing relinkctl file to be deleted before open(). */
#define MAX_RCTL_RUNDOWN_RETRIES	16	/* # of times to allow a mapped relinkctl file to get run down before shmat(). */

DEBUG_ONLY(GBLDEF int	saved_errno;)
GBLREF	uint4		process_id;
GBLREF	rtn_tabent	*rtn_names, *rtn_names_end;
GBLREF	stack_frame	*frame_pointer;
GBLREF	int		process_exiting;
OS_PAGE_SIZE_DECLARE

STATICFNDCL void relinkctl_map(open_relinkctl_sgm *linkctl);
STATICFNDCL void relinkctl_unmap(open_relinkctl_sgm *linkctl);
STATICFNDCL int relinkctl_fcntl_lock(int fd, int l_type);
STATICFNDCL void relinkctl_delete(open_relinkctl_sgm *linkctl);

#define SLASH_GTM_RELINKCTL	"/gtm-relinkctl-"
#define SLASH_GTM_RELINKCTL_LEN	STRLEN(SLASH_GTM_RELINKCTL)
#define MAX_RCTL_OPEN_RETRIES	16

error_def(ERR_FILEPARSE);
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
 *   - objpath            - object name string
 *   - objpath_alloc_len  - length of the buffer where the resolved path to the object directory is to be placed in case of MUPIP
 *                          RUNDOWN -RELINKCTL
 *
 * Output:
 *   - Found or newly created private structure which points to shared relink control structure
 */
open_relinkctl_sgm *relinkctl_attach(mstr *obj_container_name, mstr *objpath, int objpath_alloc_len)
{
	open_relinkctl_sgm 	*linkctl, new_link, *new_link_ptr;
	int			i, len, save_errno;
	mstr			objdir;
	char			pathin[GTM_PATH_MAX], resolvedpath[GTM_PATH_MAX];	/* Includes null terminator char */
	char			*pathptr;
	boolean_t		obj_dir_found;
	char			relinkctl_path[GTM_PATH_MAX], *ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef AUTORELINK_SUPPORTED
	/* Directory name normalization - the directory name must always be the same for purposes of mapping the relinkctl
	 * file. To accomplish this takes two steps:
	 *    1. Use realpath() on the directory name to disambiguate it.
	 *    2. Remove trailing slash(es) from the object directory name.
	 */
	obj_dir_found = TRUE;				/* Assume we will find the path */
	assert(GTM_PATH_MAX > obj_container_name->len);	/* Should have been checked by our caller */
	memcpy(pathin, obj_container_name->addr, obj_container_name->len);
	pathin[obj_container_name->len] = '\0';		/* Needs null termination for realpath call */
	pathptr = realpath(pathin, resolvedpath);
	if (NULL == pathptr)
	{
		if (ENOENT == (save_errno = errno))	/* Note assignment */
		{
			obj_dir_found = FALSE;		/* Path no longer exists - use our best attempt to find it */
			pathptr = pathin;
		} else
		{	/* This error is appropriate here as the directory was checked in the caller before coming here
			 * so this error is "just-in-case".
			 */
			if (NULL != objpath)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2,
						objpath->len, objpath->addr, save_errno);
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("realpath()"), CALLFROM, save_errno);
		}
	}
	objdir.addr = pathptr;
	objdir.len = STRLEN(pathptr);
	assert(objdir.len < ARRAYSIZE(resolvedpath));
	assert((0 == objpath_alloc_len) || TREF(is_mu_rndwn_rlnkctl));
	if (TREF(is_mu_rndwn_rlnkctl) && (0 < objpath_alloc_len))
	{	/* In case MUPIP RUNDOWN -RELINKCTL is done on an argument with a relative path, provide it with the full path. In
		 * case the object directory is not found, set the length of the passed mstr to 0, but only if the objpath_alloc_len
		 * argument is non-zero.
		 */
		if (!obj_dir_found)
			obj_container_name->len = 0;
		else if (objdir.len <= objpath_alloc_len)
		{
			memcpy(obj_container_name->addr, objdir.addr, objdir.len);
			obj_container_name->len = objdir.len;
		}
	}
	while ((1 < objdir.len) && ('/' == *(objdir.addr + objdir.len - 1)))
		objdir.len--;
	objdir.addr[objdir.len] = '\0';
	/* Now look the directory up in our list to see if we have it or not already. */
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
		if (MSTR_EQ(&objdir, &linkctl->zro_entry_name))
			return linkctl;
	/* Populate the relinkctl segment structure locally rather than in malloced space, so that in case relinkctl_open() below
	 * fails we do not leak memory.
	 */
	len = relinkctl_get_key(relinkctl_path, &objdir) + 1;	/* + 1 for trailing null in relinkctl_path */
	assert((len <= ARRAYSIZE(relinkctl_path)) && ('\0' == relinkctl_path[len - 1]));
	memset(&new_link, 0, SIZEOF(open_relinkctl_sgm));
	new_link.zro_entry_name = objdir;
	assert('\0' == new_link.zro_entry_name.addr[new_link.zro_entry_name.len]);
	new_link.relinkctl_path = relinkctl_path;
	assert('\0' == new_link.relinkctl_path[len - 1]);
	for (i = 0; i < NUM_RTNOBJ_SHM_INDEX; i++)
		new_link.rtnobj_shmid[i] = INVALID_SHMID;
	/* Now call relinkctl_open() to initialize new_link.fd, new_link.hdr, new_link.n_records, new_link.rec_base,
	 * new_link.shm_hashbase, and new_link.locked. Note that in situations when the object file is not found, the call will
	 * return a non-zero status, giving us a chance to return NULL.
	 *
	 * We do that because it is possible that although realpath() did not find the object and we have never opened its relinkctl
	 * file (derived in case of ZRUPDATE from the user-provided input, which might actually not be the real path), the relinkctl
	 * file was already created by a different process. Only if relinkctl_open() fails do we return NULL.
	 *
	 * Keep in mind that if the caller of relinkctl_attach() is op_zrupdate(), very shortly upon return we are going to attempt
	 * to use the errno value set above in realpath(), so we need to restore it here first.
	 *
	 * It is also possible to get a non-zero return status if the caller is MUPIP RUNDOWN -RELINKCTL and the relinkctl file does
	 * not exist and therefore does not need to be run down.
	 */
	if (0 != relinkctl_open(&new_link, !obj_dir_found))
	{
		if (!obj_dir_found)
			errno = save_errno;
		return NULL;
	}
	/* No errors were raised so far, so copy the segment information into a malloced space. */
	new_link_ptr = malloc(SIZEOF(open_relinkctl_sgm) + objdir.len + 1 + len); /* + 1 for trailing null in zro_entry_name */
	memcpy(new_link_ptr, &new_link, SIZEOF(open_relinkctl_sgm));
	ptr = (char *)(new_link_ptr + 1);
	memcpy(ptr, new_link.zro_entry_name.addr, new_link.zro_entry_name.len + 1);
	new_link_ptr->zro_entry_name.addr = ptr;
	ptr += new_link.zro_entry_name.len + 1;
	memcpy(ptr, new_link.relinkctl_path, len);
	new_link_ptr->relinkctl_path = ptr;
	/* Add to open list. */
	new_link_ptr->next = TREF(open_relinkctl_list);
	TREF(open_relinkctl_list) = new_link_ptr;
	return new_link_ptr;
#	else
	return NULL;
#	endif
}

/* Routine to open and mmap a relinkctl file for a given $ZROUTINES object directory.
 *
 * Parameter:
 *   - linkctl            - open_relinkctl_sgm-type (process-private) block describing shared (mmap'd) entry and supplying the path
 *                          and name of the relinkctl file comprised of both static and hashed directory names for uniqueness.
 *   - object_dir_missing - flag indicating whether we know for a fact that the object directory we are operating on does not exist.
 *
 * Returns:
 *    Various fields in linkctl (fd, hdr).
 *
 * The control structure should be both readable *and* writable by anything that can read the object directory.
 */
int relinkctl_open(open_relinkctl_sgm *linkctl, boolean_t object_dir_missing)
{
#	ifdef AUTORELINK_SUPPORTED
	int			fd, i, j, rc, save_errno, shmid, status, stat_res, user_id, group_id, perm;
	struct stat     	stat_buf;
	size_t			shm_size;
	boolean_t		is_mu_rndwn_rlnkctl, shm_removed, obtained_perms, rctl_existed, need_shmctl;
	relinkshm_hdr_t		*shm_base;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	relinkctl_data		*hdr;
	char			errstr[256];
	struct stat		dir_stat_buf;
	int			rctl_deleted_count, rctl_rundown_count, rctl_init_wait_count;
	struct perm_diag_data	pdd;
	struct shmid_ds		shmstat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	linkctl->hdr = NULL;
	/* Open the given relinkctl file. */
	DBGARLNK((stderr, "relinkctl_open: pid = %d : Opening relinkctl file %s for entry %.*s\n", getpid(),
		linkctl->relinkctl_path, linkctl->zro_entry_name.len, linkctl->zro_entry_name.addr));
	/* Anybody that has read permissions to the object container should have write permissions to the relinkctl file. */
	rctl_deleted_count = rctl_rundown_count = rctl_init_wait_count = 0;
	is_mu_rndwn_rlnkctl = TREF(is_mu_rndwn_rlnkctl);
	do
	{	/* We do not need to check the existence of the actual object directory if we verify that the respective relinkctl
		 * file has already been created.
		 */
		STAT_FILE(linkctl->relinkctl_path, &stat_buf, stat_res);
		if (-1 == stat_res)
		{
			if (ENOENT != errno)
			{	/* If the STAT call failed for a reason other than ENOENT, we will be unable to use the relinkctl
				 * file, so error out right away.
			 	 */
				SNPRINTF(errstr, SIZEOF(errstr), "stat() of file %s failed", linkctl->relinkctl_path);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, errno);
			} else
			{	/* If the relinkctl file is missing, then we are not going to create one if either the object
				 * directory does not exist or we are a MUPIP RUNDOWN -RELINKCTL process.
				 */
				if (object_dir_missing || is_mu_rndwn_rlnkctl)
					return -1;
				/* We have to create a relinkctl file. We derive the permissions to use based on those of the object
				 * directory.
				 */
				assert('\0' == linkctl->zro_entry_name.addr[linkctl->zro_entry_name.len]); /* For STAT_FILE. */
				STAT_FILE(linkctl->zro_entry_name.addr, &dir_stat_buf, stat_res);
				if (-1 == stat_res)
				{
					SNPRINTF(errstr, SIZEOF(errstr), "stat() of file %s failed", linkctl->zro_entry_name.addr);
					ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, errno);
				}
				if (!gtm_permissions(&dir_stat_buf, &user_id, &group_id, &perm, PERM_IPC, &pdd))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10 + PERMGENDIAG_ARG_COUNT) ERR_RELINKCTLERR, 2,
						RTS_ERROR_MSTR(&linkctl->zro_entry_name), ERR_PERMGENFAIL, 4,
						RTS_ERROR_STRING("relinkctl"), RTS_ERROR_MSTR(&linkctl->zro_entry_name),
						PERMGENDIAG_ARGS(pdd));
				}
				/* Attempt to create the relinkctl file with desired permissions. */
				OPEN3_CLOEXEC(linkctl->relinkctl_path, O_CREAT | O_RDWR | O_EXCL, perm, fd);
				obtained_perms = TRUE;
			}
		} else
			obtained_perms = FALSE;
		assert((!obtained_perms) || (!object_dir_missing));
		/* If file already existed, try to open it. */
		if ((!obtained_perms) || ((FD_INVALID == fd) && (errno == EEXIST)))
		{
			rctl_existed = TRUE;
			OPEN_CLOEXEC(linkctl->relinkctl_path, O_RDWR, fd);
		} else
			rctl_existed = FALSE;
		if (FD_INVALID == fd)
		{	/* If the file that we have previously seen is now gone, retry. However, avoid an infinite loop. */
			if (rctl_existed && (ENOENT == errno) && (MAX_RCTL_DELETED_RETRIES > rctl_deleted_count++))
				continue;
			SNPRINTF(errstr, SIZEOF(errstr), "open() of file %s failed", linkctl->relinkctl_path);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, errno);
		} else if (obtained_perms)
		{	/* If we just created the relinkctl file, ensure correct permissions on it. */
			FSTAT_FILE(fd, &stat_buf, status);
			if (0 != status)
			{
				save_errno = errno;
				CLOSEFILE_RESET(fd, rc);
				SNPRINTF(errstr, SIZEOF(errstr), "fstat() of file %s failed", linkctl->relinkctl_path);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
			if ((((INVALID_UID != user_id) && (user_id != stat_buf.st_uid))
					|| ((INVALID_GID != group_id) && (group_id != stat_buf.st_gid)))
				&& (-1 == fchown(fd, user_id, group_id)))
			{
				save_errno = errno;
				CLOSEFILE_RESET(fd, rc);
				SNPRINTF(errstr, SIZEOF(errstr), "fchown() of file %s failed", linkctl->relinkctl_path);
				ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
			}
		}
		linkctl->fd = fd;
		FTRUNCATE(fd, RELINKCTL_MMAP_SZ, rc);
		if (0 != rc)
		{
			save_errno = errno;
			SNPRINTF(errstr, SIZEOF(errstr), "ftruncate() of file %s failed", linkctl->relinkctl_path);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
		}
		relinkctl_map(linkctl);	/* linkctl->hdr is now accessible */
		hdr = linkctl->hdr;
		if (!hdr->initialized && rctl_existed && (MAX_RCTL_INIT_WAIT_RETRIES > rctl_init_wait_count++))
		{	/* The creator process has not yet initialized the shared memory for the relinkctl file; give it a chance to
			 * do so before proceeding with grabbing the lock.
			 */
			relinkctl_unmap(linkctl);
			SLEEP_USEC(1000, FALSE);
			continue;
		}
		relinkctl_lock_exclu(linkctl);
		if (hdr->file_deleted)
		{	/* Some other process concurrently opened and closed/rundown the relinkctl file while we were still inside
			 * relinkctl_open(). Reattach to new file (create it if necessary) instead of using the current fd as this
			 * will not be visible to other new processes.
			 */
			DBGARLNK((stderr, "relinkctl_open: file_deleted = 1. Retrying open.\n"));
			relinkctl_unlock_exclu(linkctl);
			relinkctl_unmap(linkctl);
			assert(NULL == linkctl->hdr);
			assertpro(MAX_RCTL_RUNDOWN_RETRIES > rctl_rundown_count++); /* Too many loops should not be possible. */
			continue;
		}
		if (0 == hdr->relinkctl_max_rtn_entries)
		{
			hdr->relinkctl_max_rtn_entries = TREF(gtm_autorelink_ctlmax);
			hdr->relinkctl_hash_buckets = getprime(TREF(gtm_autorelink_ctlmax));
		} else
			assert(hdr->relinkctl_hash_buckets == getprime(hdr->relinkctl_max_rtn_entries));
		shm_size = RELINKCTL_SHM_SIZE(hdr->relinkctl_hash_buckets, hdr->relinkctl_max_rtn_entries);
		ADJUST_SHM_SIZE_FOR_HUGEPAGES(shm_size, shm_size); /* Second parameter "shm_size" is adjusted size */
		if (hdr->initialized)
		{	/* There is an existing shared memory segment, to which we need to attach. Need lock (already obtained) to
			 * prevent interaction with concurrent initialization, nattached = 0.
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
				/* If shm has been removed, then direct one to use MUPIP RUNDOWN -RELINKCTL. */
				if (!is_mu_rndwn_rlnkctl)
					relinkctl_lock_exclu(linkctl);
				if (!shm_removed || !is_mu_rndwn_rlnkctl)
				{
					hdr->nattached--;
					relinkctl_unlock_exclu(linkctl);
					relinkctl_unmap(linkctl);
					SNPRINTF(errstr, SIZEOF(errstr), "shmat() failed for shmid=%d shmsize=%llu [0x%llx]",
						shmid, shm_size, shm_size);
					if (!shm_removed)
						ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, errstr, save_errno);
					else
						ISSUE_REQRLNKCTLRNDWN_SYSCALL(linkctl, errstr, save_errno);
				} else
				{	/* This is MUPIP RUNDOWN -RELINKCTL and shm is removed. There is no point creating one. */
					DBGARLNK((stderr, "relinkctl_open: Set hdr->relinkctl_shmid to INVALID_SHMID\n"));
					hdr->relinkctl_shmid = INVALID_SHMID;
					return 0;
				}
			}
		} else if (!is_mu_rndwn_rlnkctl)
		{	/* We have come here ahead of the process that has created the relinkctl file even though we have slept many
			 * times, giving the creator a chance to grab the lock. So if we did not obtain the permissions of the
			 * object directory, we have no permissions to apply on a shared memory segment we are about to create;
			 * therefore, error out.
			 */
			if (!obtained_perms)
			{
				relinkctl_unlock_exclu(linkctl);
				relinkctl_unmap(linkctl);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REQRLNKCTLRNDWN, 3,
						linkctl->relinkctl_path, RTS_ERROR_MSTR(&linkctl->zro_entry_name));
			}
			DBGARLNK((stderr, "relinkctl_open: file first open\n"));
			hdr->n_records = 0;
			/* Create shared memory to store hash buckets of routine names for faster search in relinkctl file */
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
			assert(obtained_perms);
			/* Change uid, group-id and permissions if needed */
			need_shmctl = FALSE;
			if ((INVALID_UID != user_id) && (user_id != shmstat.shm_perm.uid))
			{
				shmstat.shm_perm.uid = user_id;
				need_shmctl = TRUE;
			}
			if ((INVALID_GID != group_id) && (group_id != shmstat.shm_perm.gid))
			{
				shmstat.shm_perm.gid = group_id;
				need_shmctl = TRUE;
			}
			if (shmstat.shm_perm.mode != perm)
			{
				shmstat.shm_perm.mode = perm;
				need_shmctl = TRUE;
			}
			if (need_shmctl && (-1 == shmctl(shmid, IPC_SET, &shmstat)))
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
			/* Since search for a routine proceeds to check all rtnobj shmids from rtnobj_min_shm_index to
			 * rtnobj_max_shm_index, set the two to impossible values so creation of the first rtnobj shmid
			 * (whatever its shm_index turns out to be) causes these two to be overwritten to that shm_index.
			 * Since rtnobj_min_shm_index is overwritten only if it is greater than shm_index, we set it to
			 * one more than the highest value possible for shm_index i.e. NUM_RTNOBJ_SHM_INDEX. Likewise for
			 * rtnobj_max_shm_index.
			 */
			shm_base->rtnobj_min_shm_index = NUM_RTNOBJ_SHM_INDEX;
			shm_base->rtnobj_max_shm_index = 0;
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
			/* Shared memory initialization complete. */
			hdr->initialized = TRUE;
			relinkctl_unlock_exclu(linkctl);
		} else
		{	/* This is MUPIP RUNDOWN -RELINKCTL and relinkctl file exists but the shared memory does not. We are not
			 * going to create the shared memory to only later run it down.
			 */
			DBGARLNK((stderr, "relinkctl_open: Set hdr->relinkctl_shmid to INVALID_SHMID\n"));
			hdr->relinkctl_shmid = INVALID_SHMID;
			return 0;
		}
		assert(linkctl->locked == is_mu_rndwn_rlnkctl);
		assert(0 == ((UINTPTR_T)shm_base % 8));
		assert(0 == (SIZEOF(relinkshm_hdr_t) % SIZEOF(uint4)));	/* assert SIZEOF(*sm_uint_ptr_t) alignment */
		shm_base->skip_rundown_check = TRUE;
		linkctl->shm_hashbase = (sm_uint_ptr_t)(sm_uc_ptr_t)(shm_base + 1);
			/* Skip past shm header to reach hash array start */
		assert(0 == ((UINTPTR_T)linkctl->shm_hashbase % 8));	/* assert each section is 8-byte aligned at least */
		/* RELINKSHM_RTNHASH_SIZE definition relies on this for 8-byte alignment */
		linkctl->rec_base = (relinkrec_t *)((sm_uc_ptr_t)linkctl->shm_hashbase
						    + RELINKSHM_RTNHASH_SIZE(hdr->relinkctl_hash_buckets));
		assert(128 >= SIZEOF(relinkrec_t));	/* or else adjust CACHELINE_PAD_COND filler downwards */
		assert(0 == ((UINTPTR_T)linkctl->rec_base % 8));	/* assert each section is 8-byte aligned at least */
		break;
	} while (TRUE);
#	endif
	return 0;
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
	char		*key_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gtmmrhash_128(zro_entry_name->addr, zro_entry_name->len, 0, &hash);
	gtmmrhash_128_hex(&hash, hexstr);
	hexstr[32] = '\0';
	/* If the cumulative path to the relinkctl file exceeds GTM_PATH_MAX, it will be inaccessible, so no point continuing. */
	if (GTM_PATH_MAX < (TREF(gtm_linktmpdir)).len + SLASH_GTM_RELINKCTL_LEN + SIZEOF(hexstr))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(zro_entry_name),
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Path to the relinkctl file is too long"));
	key_ptr = key;
	memcpy(key_ptr, (TREF(gtm_linktmpdir)).addr, (TREF(gtm_linktmpdir)).len);
	key_ptr += (TREF(gtm_linktmpdir)).len;
	STRCPY(key_ptr, SLASH_GTM_RELINKCTL);
	key_ptr += SLASH_GTM_RELINKCTL_LEN;
	STRNCPY_STR(key_ptr, hexstr, 33); /* NULL-terminate the string. */
	key_ptr += 32;
	keylen = (key_ptr - key);
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
/*
 * Relinkctl file record management routines
 */

/* Iterate through each relink_record_struct starting at (relink_record_ptr)&linkctl->map_addr[0]
 * Find rec such that rec->rtnname == rtnname, return offset of rec.
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
	assert(linkctl->hdr->relinkctl_hash_buckets > hash);
	ptr += hash;
	prev_index = *ptr;
	index = prev_index - 1;	/* 'index' is unsigned so will become huge positive number in case *ptr is 0 */
	base = linkctl->rec_base;
	nrec = linkctl->n_records;
	assert(linkctl->hdr->relinkctl_max_rtn_entries >= nrec);
	while (index < nrec)
	{
		rec = &base[index];
		DEBUG_ONLY(COMPUTE_RELINKCTL_HASH(rtnname, rtnhash, linkctl->hdr->relinkctl_hash_buckets);)
		assert(rtnhash == hash);
		/* Check routine name plus null trailer in fixed version */
		if ((0 == memcmp(&rec->rtnname_fixed.c, rtnname->addr, rtnname->len))
				&& ('\0' == rec->rtnname_fixed.c[rtnname->len]))
			return rec;
		assert(linkctl->hdr->relinkctl_max_rtn_entries >= rec->hashindex_fl);
		prev_index = index + 1;
		index = rec->hashindex_fl - 1;
	}
	*prev_hash_index = prev_index;
	return NULL;
}

/* Like relinkctl_find_record, but inserts a new entry instead of returning NULL */
relinkrec_t *relinkctl_insert_record(open_relinkctl_sgm *linkctl, mstr *rtnname)
{
	relinkrec_t	*base, *rec;
	uint4		hash, prev_hash_index, nrec;

	COMPUTE_RELINKCTL_HASH(rtnname, hash, linkctl->hdr->relinkctl_hash_buckets);
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
			if (linkctl->hdr->relinkctl_max_rtn_entries == linkctl->hdr->n_records)
			{
				relinkctl_unlock_exclu(linkctl);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RELINKCTLFULL, 3, linkctl->zro_entry_name.len,
					      linkctl->zro_entry_name.addr, linkctl->hdr->relinkctl_max_rtn_entries);
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
	boolean_t		is_mu_rndwn_rlnkctl, remove_shm, remove_rctl;
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
		{	/* MUPIP RUNDOWN -RELINKCTL should still hold a lock. */
			assert(linkctl->locked == is_mu_rndwn_rlnkctl);
			if (!is_mu_rndwn_rlnkctl)
				relinkctl_lock_exclu(linkctl);
			hdr = linkctl->hdr;
			assert((INVALID_SHMID != hdr->relinkctl_shmid) || is_mu_rndwn_rlnkctl);
			if (INVALID_SHMID != hdr->relinkctl_shmid)
			{
				assert(0 < hdr->nattached);
				if (0 < hdr->nattached)
					hdr->nattached--;
				nattached = hdr->nattached;
				DBGARLNK((stderr, "relinkctl_rundown : pid = %d : file %s : post-decr nattached = %d\n",
					getpid(), linkctl->relinkctl_path, nattached));
				assert(0 <= nattached);
				shm_hdr = GET_RELINK_SHM_HDR(linkctl);
			} else
				nattached = -1;
			if (0 == nattached)
			{
				DBGARLNK((stderr, "relinkctl_rundown : nattached = 0\n"));
				remove_shm = remove_rctl = TRUE;
			} else if (is_mu_rndwn_rlnkctl)
			{	/* If MUPIP RUNDOWN -RELINKCTL, check if shm_buff.nattch is 1 (i.e. mupip rundown -relinkctl
				 * is the only one attached to this shm). If so, ignore nattached and run this shm down.
				 * Most likely processes that had bumped nattached got kill -9ed.
				 * If shm_buff.nattch is not 1, fix hdr->nattached to match shm_buf.nattch so when the time
				 * comes for the last GTM process to rundown, it will remove the shm without the need for
				 * any more MUPIP RUNDOWN -RELINKCTL commands.
				 */
				shmid = hdr->relinkctl_shmid;
			 	if (INVALID_SHMID != hdr->relinkctl_shmid)
				{
					if (0 != shmctl(shmid, IPC_STAT, &shm_buf))
					{
						assert(FALSE);
						remove_shm = remove_rctl = FALSE;
					} else
					{
						nattached = shm_buf.shm_nattch - 1; /* remove self since we will do a SHMDT soon */
						if (hdr->nattached != nattached)
						{
							hdr->nattached = nattached;	/* fix hdr->nattached while at this */
							shm_hdr->rndwn_adjusted_nattch = TRUE;
						}
						remove_shm = remove_rctl = !nattached;
					}
					if (!remove_shm)
					{
						assert(nattached);
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RLNKCTLRNDWNFL, 3,
									RTS_ERROR_MSTR(&linkctl->zro_entry_name), nattached);
					}
				} else
				{
					remove_shm = FALSE;
					remove_rctl = TRUE;
				}
			} else
			{
				remove_shm = FALSE;
				remove_rctl = FALSE;
			}
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
							/* We expect no one else to be attached to the rtnobj shm. One case we
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
			} else
			{
				SHMDT(shm_hdr);		/* If error detaching, not much we can do. Just move on */
			}
			if (remove_rctl)
			{
				relinkctl_delete(linkctl);
				if (is_mu_rndwn_rlnkctl)
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RLNKCTLRNDWNSUC, 2,
							RTS_ERROR_MSTR(&linkctl->zro_entry_name));
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
