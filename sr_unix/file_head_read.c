/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtmio.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "file_head_read.h"
#include "gtmmsg.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

/*
 * This is a plain way to read file header.
 * User needs to take care of concurrency issue etc.
 * Parameters :
 *	fn : full name of a database file.
 *	header: Pointer to database file header structure (may not be in shared memory)
 *	len: size of header (may be just SGMNT_HDR_LEN or SIZEOF_FILE_HDR_MAX)
 */
boolean_t file_head_read(char *fn, sgmnt_data_ptr_t header, int4 len)
{
	int 		save_errno, fd, header_size;
	struct stat	stat_buf;
	ZOS_ONLY(int	realfiletag;)

	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_TEXT);
	ZOS_ONLY(error_def(ERR_BADTAG);)

	header_size = SIZEOF(sgmnt_data);
	OPENFILE(fn, O_RDONLY, fd);
	if (FD_INVALID == fd)
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(fn, errno, realfiletag, TAG_BINARY);
#endif
	FSTAT_FILE(fd, &stat_buf, save_errno);
	if (-1 == save_errno)
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
 		CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
		return FALSE;
	}
	if (!S_ISREG(stat_buf.st_mode) || stat_buf.st_size < header_size)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBNOTGDS, 2, LEN_AND_STR(fn));
 		CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
		return FALSE;
	}
	LSEEKREAD(fd, 0, header, header_size, save_errno);
	if (0 != save_errno)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
 		CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
		return FALSE;
	}
	if (memcmp(header->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBNOTGDS, 2, LEN_AND_STR(fn));
 		CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
		return FALSE;
	}
	CHECK_DB_ENDIAN(header, strlen(fn), fn);
	assert(MASTER_MAP_SIZE_MAX >= MASTER_MAP_SIZE(header));
	assert(SGMNT_HDR_LEN == len || SIZEOF_FILE_HDR(header) <= len);
	if (SIZEOF_FILE_HDR(header) <= len)
	{
		LSEEKREAD(fd, ROUND_UP(SGMNT_HDR_LEN + 1, DISK_BLOCK_SIZE), MM_ADDR(header), MASTER_MAP_SIZE(header), save_errno);
		if (0 != save_errno)
		{
			gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
			CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
			return FALSE;
		}
	}
	CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
	if (0 != save_errno)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	return TRUE;
}
