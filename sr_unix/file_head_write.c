/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

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
#include "file_head_write.h"
#include "gtmmsg.h"

/*
 * This is a plain way to write file header to database.
 * User needs to take care of concurrency issue etc.
 * Parameters :
 *	fn : full name of a database file.
 *	header: Pointer to database file header structure (may not be in shared memory)
 */
boolean_t file_head_write(char *fn, sgmnt_data *header)
{
	int 		save_errno, fd, header_size;

	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBNOTGDS);

	header_size = sizeof(sgmnt_data);
	if (-1 == (fd = OPEN(fn, O_RDWR)))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	LSEEKWRITE(fd, 0, header, header_size, save_errno);
	if (0 != save_errno)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	CLOSEFILE(fd, save_errno);
	if (0 != save_errno)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	return TRUE;
}
