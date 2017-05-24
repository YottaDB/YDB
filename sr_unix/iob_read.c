/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iob_read - buffered I/O with GT.M error signaling

   void iob_read(BFILE *bf, void *buf, int nbytes)

   BFILE *bf:	file to read
   buf:		buffer to place data read;
   nbytes:	bytes to read.

*/
#include "mdef.h"

#include "gtm_string.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "error.h"
#include "iob.h"
#include "gtmio.h"
#include "gtmmsg.h"

GBLREF	uint4		restore_read_errno;

error_def(ERR_IOEOF);

void iob_read(BFILE *bf, char *buf, int nbytes)
{
	int4	nread, nreread;
	int	rc;

	if (bf->write_mode)
	{
		iob_flush(bf);
		bf->write_mode = FALSE;
	}
#	ifdef DEBUG_IOB
	PRINTF("iob_read:\tiob_read(%x, %x, %d), bf->remaining = %d\n", bf, buf, nbytes, bf->remaining);
#	endif
	while (nbytes > bf->remaining)
	{
		/* copy bytes remaining in buffer */
		memcpy(buf, bf->bptr, bf->remaining);
		nbytes -= bf->remaining;
		buf += bf->remaining;

		/* refill */
		DOREADRL(bf->fd, bf->buf, bf->bufsiz, nread);
#		ifdef DEBUG_IOB
		PRINTF("iob_read:\t\tread(%d, %x, %d) = %d\n", bf->fd, bf->buf, bf->bufsiz, nread);
#		endif
		bf->bptr = bf->buf;
		bf->remaining = nread;
		/* In case of error, set global variable "restore_read_errno" and return to caller ("mupip_restore")
		 * for it to do cleanup before exiting.
		 */
		if ((-1 == nread) || (0 == nread) || (nread % bf->blksiz))
		{
			restore_read_errno = ((-1 == nread) ? errno : ERR_IOEOF);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) restore_read_errno);
			CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
			return;
		}
	}
	memcpy(buf, bf->bptr, nbytes);
	bf->bptr += nbytes;
	bf->remaining -= nbytes;
}
