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
#include "zhist.h"
#include "md5hash.h"
#include "md5_digest2hex.h"
#include "stringpool.h"

/*
 * This module contains routines that maintain autorelink 'relinkctl' structures.
 * TODO - add description
 */

DEBUG_ONLY(GBLDEF int	saved_errno;)
GBLREF	uint4	process_id;

STATICFNDCL void relinkctl_map(open_relinkctl_sgm *linkctl, uint4 n_records);
STATICFNDCL void relinkctl_unmap(open_relinkctl_sgm *linkctl);
STATICFNDCL uint4 relinkctl_recs2map(uint4 n_records);
STATICFNDCL int relinkctl_fcntl_lock(int fd, int l_type);
STATICFNDCL void relinkctl_get_key(char key[GTM_PATH_MAX], mstr *zro_entry_name);
STATICFNDCL void relinkctl_delete(open_relinkctl_sgm *linkctl);

#define ISSUE_RELINKCTLERR_SYSCALL(ZRO_ENTRY_NAME, LIBCALL)							\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(ZRO_ENTRY_NAME),		\
		      ERR_SYSCALL, 5, LEN_AND_STR(LIBCALL), CALLFROM, DEBUG_ONLY(saved_errno = )errno)

error_def(ERR_RELINKCTLERR);
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
	int			len, save_errno;
	mstr			objdir;
	char			pathin[GTM_PATH_MAX], resolvedpath[GTM_PATH_MAX];	/* Includes null terminator char */
	char			*pathptr;
	boolean_t		pathfound;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef USHBIN_SUPPORTED
	/* Directory name normalization - the directory name must always be the same for purposes of mapping the relinkctl
	 * file. To accomplish this takes two steps:
	 *    1. Use realpath() on the directory name to disambiguate it.
	 *    2. Remove trailing slash(es) in the object directory name.
	 */
	pathfound = TRUE;				/* Assume we'll find the path */
	assert(GTM_PATH_MAX > obj_container_name->len);	/* Should have been checked by our caller */
	memcpy(pathin, obj_container_name->addr, obj_container_name->len);
	pathin[obj_container_name->len] = '\0';		/* Needs null termination for realpath call */
	pathptr = realpath(pathin, resolvedpath);
	if (NULL == pathptr)
	{
		pathfound = FALSE;			/* Path no longer exists - use our best attempt to find it */
		pathptr = pathin;
	}
	objdir.addr = pathptr;
	objdir.len = strlen(pathptr);
	while ((1 < objdir.len) && ('/' == *(objdir.addr + objdir.len - 1)))
		objdir.len--;
	/* Now look the directory up in our list to see if we have it or not already */
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
	{
		if (MSTR_EQ(&objdir, &linkctl->zro_entry_name))
			return linkctl;
	}
	/* If realpath() didn't find the path and we don't already have it open, don't create a relinkctl file for it */
	if (!pathfound)
		return NULL;
	/* Not already open */
	new_link = malloc(SIZEOF(open_relinkctl_sgm));
	new_link->zro_entry_name = objdir;
	s2pool(&new_link->zro_entry_name);		/* Migrate string to stringpool for safe keeping */
	/* Open + map structure */
	relinkctl_open(new_link);
	/* Add to open list */
	new_link->next = TREF(open_relinkctl_list);
	TREF(open_relinkctl_list) = new_link;
	return new_link;
#	else
	return NULL;
#	endif

}

/*
 * Given linkctl->zro_entry_name -- an object directory in $ZROUTINES -- open() and mmap() relink ctl structure, thus
 * filling in linkctl->fd and linkctl->hdr.
 * The control structure should be both readable *and* writable by anything that can read the object directory.
 */
void relinkctl_open(open_relinkctl_sgm *linkctl)
{
	int		fd, status, save_errno;
	int		umask_creat, umask_orig;
	char		relinkctl_path[GTM_PATH_MAX];
	struct stat     stat_buf;

#	ifdef USHBIN_SUPPORTED
	linkctl->hdr = NULL;
	/* open the given relinkctl file */
	relinkctl_get_key(relinkctl_path, &linkctl->zro_entry_name);
	DBGARLNK((stderr, "relinkctl_open: Opening relinkctl file %s for entry %.*s\n", relinkctl_path,
		  linkctl->zro_entry_name.len, linkctl->zro_entry_name.addr));
	/* RW permissions for owner and others as determined by umask. */
	umask_orig = umask(000);	/* determine umask (destructive) */
	(void)umask(umask_orig);	/* reset umask */
	umask_creat = 0666 & ~umask_orig;
	/* Anybody that has read permissions to the object container should have write permissions to the relinkctl file */
	fd = OPEN3(relinkctl_path, O_CREAT | O_RDWR, umask_creat);
	/* TODO: get permissions right. see comment above . important */
	/* Do we want a relinkctl sitting around for $gtm_dist? */
	if (FD_INVALID == fd)
		ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, "open()");
	linkctl->fd = fd;
	/* If file is newly created, initialize it */
	FSTAT_FILE(fd, &stat_buf, status);
	if (0 != status)
		ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, "fstat()");
	if (0 == stat_buf.st_size)
	{
		relinkctl_lock_exclu(linkctl);
		FSTAT_FILE(fd, &stat_buf, status);
		if (0 != status)
		{
			relinkctl_unlock_exclu(linkctl);
			ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, "fstat()");
		}
		if (0 == stat_buf.st_size)
		{
			/* TODO: fix file extension so we don't need to preallocate so much space */
			ftruncate(fd, SIZEOF(relinkctl_data) + 1000000 * SIZEOF(relinkrec_t));
			relinkctl_map(linkctl, 0);
			linkctl->hdr->n_records = 0;	/* what happens if kill -9'd between ftruncate and n_records = 0? */
			SET_LATCH_GLOBAL(&linkctl->hdr->attach_latch, LOCK_AVAILABLE);
			relinkctl_unmap(linkctl);
		}
		relinkctl_unlock_exclu(linkctl);
	}
	/* Do an small mapping in order to read n_records, then map the full file */
	relinkctl_map(linkctl, 0);
	relinkctl_ensure_fullmap(linkctl);
	/* Need lock to prevent interaction with concurrent initialization, nattached = 0 */
	relinkctl_lock_exclu(linkctl);
	linkctl->hdr->nattached++;
	/*sprintf(buff, "%s - incremented. nattached = %d", linkctl->zro_entry_name, linkctl->hdr->nattached);
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));*/
 	/*send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
	  LEN_AND_LIT("%s - incremented. nattached = %d")); debugging TODO*/
	relinkctl_unlock_exclu(linkctl);
	/* TODO: when to close? with ZGOTO 0? or never bother? */
#	endif
}

/* Routine to generate unique key for a $ZROUTINES entry name used to create relinkctl file for that entry in the directory
 * $gtm_linktmpdir (e.g. /testarea1/gtm/temp --> $gtm_linktmpdir/gtm-relinkctl-d0f3d074c724430bc1c7679141b96411).
 * Theoretically, we'd need a scheme to resolve hash collisions. Say, append -<collision_id> to the key.
 * But since this is MD5, we can assume a collision will never happen in practice, so we do not handle the extremely
 * unlikely event of an MD5 hash collision for the few $ZROUTINES entries used by processes using the same $gtm_linktmpdir
 * value.
 *
 * Parameters:
 *
 *   key            - Generated as $gtm_linktmpdir/gtm-relinkctl-<md5>. Buffer should be GTM_PATH_MAX bytes (output).
 *   zro_entry_name - Address of mstr containing the fully expanded zroutines entry directory name.
 */
#ifdef USHBIN_SUPPORTED
STATICFNDEF void relinkctl_get_key(char key[GTM_PATH_MAX], mstr *zro_entry_name)
{
	cvs_MD5_CTX	md5context;
	unsigned char	digest[MD5_DIGEST_LENGTH];
	char		hexstr[MD5_HEXSTR_LENGTH];
	int		keylen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cvs_MD5Init(&md5context);
	cvs_MD5Update(&md5context, (const unsigned char *)zro_entry_name->addr, zro_entry_name->len);
	cvs_MD5Final(digest, &md5context);
	md5_digest2hex(hexstr, digest);
	/* Improve efficiency - use memcpy to build string and provide better max-length checking than an assert */
	keylen = SNPRINTF(&key[0], GTM_PATH_MAX - 1, "%.*s/gtm-relinkctl-%s", (TREF(gtm_linktmpdir)).len,
			  (TREF(gtm_linktmpdir)).addr, hexstr);
	assert((0 < keylen) && (GTM_PATH_MAX > keylen));
}
#endif

/**
 * Relinkctl mmap-related methods
 */

/*
 * Remap, if entire array of relink records is not mapped.
 */
void relinkctl_ensure_fullmap(open_relinkctl_sgm *linkctl)
{
	uint4	nrec;

#	ifdef USHBIN_SUPPORTED
	nrec = linkctl->hdr->n_records;
	if (nrec >= relinkctl_recs2map(linkctl->n_records))
	{
		relinkctl_unmap(linkctl);
		relinkctl_map(linkctl, nrec);
	}
	linkctl->n_records = nrec;
#	endif
}

/*
 * Map at least n_records, currently known number of entries.
 *
 * Fills in:
 * 	linkctl->hdr
 * 	linkctl->n_records
 * 	linkctl->rec_base
 */
#ifdef USHBIN_SUPPORTED
STATICFNDEF void relinkctl_map(open_relinkctl_sgm *linkctl, uint4 n_records)
{
	sm_uc_ptr_t	addr;
	size_t		mapsz;

	mapsz = SIZEOF(relinkctl_data) + (relinkctl_recs2map(n_records) * SIZEOF(relinkrec_t));
	addr = (sm_uc_ptr_t)mmap(NULL, mapsz, (PROT_READ + PROT_WRITE), MAP_SHARED, linkctl->fd, 0);
	if (MAP_FAILED == addr)
		ISSUE_RELINKCTLERR_SYSCALL(&linkctl->zro_entry_name, "mmap()");
	linkctl->hdr = (relinkctl_data *)addr;
	linkctl->n_records = n_records;
	linkctl->rec_base = &linkctl->hdr->base[0];
}

STATICFNDEF void relinkctl_unmap(open_relinkctl_sgm *linkctl)
{
	/* TODO */
}

/*
 * We don't want to repeatedly re-mmap the file, so given some minimum number of records we need to map,
 * compute a larger number to accomodate before we need to remap.
 */
STATICFNDEF uint4 relinkctl_recs2map(uint4 n_records)
{
	/* TODO: Improve - right now, limited to fixed initial size */
	return 1000000;
	/* return n_records + 1; - This causes excessive remapping - pick a better increment */
}
#endif

/**
 * Exclusive locking methods controlling WRITE access to relinkctl control files.
 */

/*
 * Routine to exclusively lock the relinkctl file.
 *
 * Parameter:
 *
 *   linkctl - address of relink control structure for a given $ZROUTINEs node
 */
void relinkctl_lock_exclu(open_relinkctl_sgm *linkctl)
{
	int	status;

#	ifdef USHBIN_SUPPORTED
	status = relinkctl_fcntl_lock(linkctl->fd, F_WRLCK);
	assert(0 == status);	/* TODO: Deal with error */
	linkctl->locked = TRUE;
#	endif
	return;
}

void relinkctl_unlock_exclu(open_relinkctl_sgm *linkctl)
{
	int	status;

#	ifdef USHBIN_SUPPORTED
	status = relinkctl_fcntl_lock(linkctl->fd, F_UNLCK);
	assert(0 == status);	/* TODO: Deal with error */
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
#ifdef USHBIN_SUPPORTED
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

/**
 * Relinkctl file record management routines
 */

/* Like relinkctl_find_record, but inserts a new entry instead of returning REC_NOT_FOUND.
 */
relinkrec_loc_t relinkctl_insert_record(open_relinkctl_sgm *linkctl, mstr *rtnname)
{
	relinkrec_loc_t		rec;
	relinkrec_ptr_abs_t	base, newrec;

#	ifdef USHBIN_SUPPORTED
	rec = relinkctl_find_record(linkctl, rtnname);
	if (REC_NOT_FOUND == rec)
	{
		relinkctl_lock_exclu(linkctl);
		rec = relinkctl_find_record(linkctl, rtnname);
		if (REC_NOT_FOUND == rec)
		{	/* File expansion scenario - TODO - don't assume additional record is available */
			assert(linkctl->locked);
			assert(linkctl->n_records == linkctl->hdr->n_records);
			base = linkctl->rec_base;
			newrec = base + (UINTPTR_T)linkctl->n_records;
			newrec->cycle = 1;	/* Start at cycle 1 instead of 0 */
			memset(&newrec->rtnname_fixed.c[0], 0, SIZEOF(mident_fixed));
			assert(MAX_MIDENT_LEN >= rtnname->len);
			memcpy(&newrec->rtnname_fixed.c[0], rtnname->addr, rtnname->len);
			/* TODO: Insert record's AVL node into AVL tree or hash entry into htab */
			linkctl->hdr->n_records++;
			rec = RCTLABS2REL(newrec, base);
			assert(rec == relinkctl_find_record(linkctl, rtnname));
		}
		relinkctl_unlock_exclu(linkctl);
	}
	return rec;
#	else
	return 0;
#	endif
}

/*
 * Iterate through each relink_record_struct starting at (relink_record_ptr)&linkctl->map_addr[0]
 * Find rec s.t. rec->rtnname == rtnname, return offset of rec.
 * Otherwise, return NOMATCH (defined 0xffff..).
 */
relinkrec_loc_t relinkctl_find_record(open_relinkctl_sgm *linkctl, mstr *rtnname)
{
	relinkrec_ptr_abs_t		rec, base, top;

#	ifdef USHBIN_SUPPORTED
	relinkctl_ensure_fullmap(linkctl);	/* Make sure we search among all currently existing records */
	base = linkctl->rec_base;
	top = base + (UINTPTR_T)linkctl->n_records;
	for (rec = base; rec < top; rec++)
	{	/* For each record, check routine name plus null trailer in fixed version */
		if ((0 == memcmp(&rec->rtnname_fixed.c, rtnname->addr, rtnname->len))
		    && ('\0' == rec->rtnname_fixed.c[rtnname->len]))
			return RCTLABS2REL(rec, base);
	}
#	endif
	return REC_NOT_FOUND;
}

/**
 * Relinkctl file rundown routines
 */

/*
 * Close all file descriptors associated with relinkctl structs, and (atomically) decrement nattached in file header.
 */
void relinkctl_rundown(boolean_t decr_attached)
{
#	ifdef USHBIN_SUPPORTED
	int			rc;
	open_relinkctl_sgm	*linkctl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (linkctl = TREF(open_relinkctl_list); NULL != linkctl; linkctl = linkctl->next)
	{
		if (decr_attached)
		{
			relinkctl_lock_exclu(linkctl);
			linkctl->hdr->nattached--;
			/*sprintf(buff, "%s - decremented. nattached = %d", linkctl->zro_entry_name, linkctl->hdr->nattached);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));*/
			/*send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
	                                                        LEN_AND_STR(linkctl->zro_entry_name));
	 		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
	                                                        LEN_AND_LIT("decremented")); debugging TODO */
			relinkctl_unlock_exclu(linkctl);
			assert(0 <= linkctl->hdr->nattached);
			if (0 == linkctl->hdr->nattached)
			{
				relinkctl_lock_exclu(linkctl);
				if (0 == linkctl->hdr->nattached)
					relinkctl_delete(linkctl);
			}
		}
		CLOSEFILE_RESET(linkctl->fd, rc);
		linkctl->hdr = NULL;
		linkctl->rec_base = NULL;
	}
	TREF(open_relinkctl_list) = NULL;
#	endif
	return;
}

/*
 * Clean up (i.e. delete) relinkctl file
 */
#ifdef USHBIN_SUPPORTED
STATICFNDEF void relinkctl_delete(open_relinkctl_sgm *linkctl)
{
	char	relinkctl_path[GTM_PATH_MAX + 1];

	relinkctl_get_key(relinkctl_path, &linkctl->zro_entry_name);
	UNLINK(relinkctl_path);
	return;
}
#endif
