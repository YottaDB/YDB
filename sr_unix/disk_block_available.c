/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* disk_block_available(int fd, int4 *ret, booleat_t fill_unix_holes)
 *   	parameter:
 *		fd:	file descriptor of the file that is located
 * 			on the disk being examined
 * 		*ret:	address to put the number of available blocks
 * 		fill_unix_holes
 * 			TRUE  ==> 	the calculation takes the
 * 					hole in this file into consideration
 * 			FALSE ==>	plain system service
 *	return:
 *		0:	upon successful completion, the number of available
 *			blocks is put to ret.
 *		errno:	as to why the request failed.
 * 		note that DISK_BLOCK_SIZE is used here instead of GDS_BLOCK_SIZE,
 * 		this is due to the intension to make this routine more general
 */

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_statvfs.h"
#ifndef __MVS__
#if !defined(sun) && !defined(__CYGWIN__)
#include <sys/dir.h>
#endif
#include <sys/param.h>
#else
#define DEV_BSIZE	fstat_buf.st_blksize
#endif


#include "have_crit.h"
#include "eintr_wrappers.h"
#include "disk_block_available.h"

int4 disk_block_available(int fd, gtm_uint64_t *ret, boolean_t fill_unix_holes)
{
	struct	stat		fstat_buf;
	struct	statvfs		fstatvfs_buf;
	int			status;

	FSTATVFS_FILE(fd, &fstatvfs_buf, status);
	if (-1 == status)
		return errno;
	*ret = (gtm_uint64_t)((fstatvfs_buf.f_frsize / DISK_BLOCK_SIZE) * fstatvfs_buf.f_bavail);
	if (fill_unix_holes)
	{
		FSTAT_FILE(fd, &fstat_buf, status);
		if (-1 == status)
			return errno;
		*ret -= (gtm_uint64_t)(DEV_BSIZE / DISK_BLOCK_SIZE
			* (DIVIDE_ROUND_UP(fstat_buf.st_size, DEV_BSIZE) - fstat_buf.st_blocks));
	}
	return 0;
}
