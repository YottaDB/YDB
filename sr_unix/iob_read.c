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
void iob_flush(BFILE *bf);

void iob_read(BFILE *bf, char *buf, int nbytes)
{
	int4	nread, nreread;
	int	rc;

	error_def(ERR_IOEOF);

	if (bf->write_mode)
	{
		iob_flush(bf);
		bf->write_mode = FALSE;
	}

#ifdef DEBUG_IOB
	PRINTF("iob_read:\tiob_read(%x, %x, %d), bf->remaining = %d\n",
	       bf, buf, nbytes, bf->remaining);
#endif

	while (nbytes > bf->remaining)
	{
		/* copy bytes remaining in buffer */
		memcpy(buf, bf->bptr, bf->remaining);
		nbytes -= bf->remaining;
		buf += bf->remaining;

		/* refill */
		DOREADRL(bf->fd, bf->buf, bf->bufsiz, nread);
#ifdef DEBUG_IOB
		PRINTF("iob_read:\t\tread(%d, %x, %d) = %d\n", bf->fd, bf->buf,
		       bf->bufsiz, nread);
#endif
		bf->bptr = bf->buf;
		bf->remaining = nread;

		if (nread == -1)
		{
			rts_error(VARLSTCNT(1) errno);
			return;
		}
		else if (nread == 0 || nread % bf->blksiz)
		{
			CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
			rts_error(VARLSTCNT(1) ERR_IOEOF);

			/* if we continued from here, assume that this is a magnetic
			   tape and we have loaded the next volume. Re-open and
			   finish the read operation.
			   */
			while (FD_INVALID == (bf->fd = OPEN3(bf->path,bf->oflag,bf->mode)))
				rts_error(VARLSTCNT(1) errno);

			DOREADRL(bf->fd, bf->buf + nread, bf->bufsiz - nread, nreread);
#ifdef DEBUG_IOB
			PRINTF("iob_read:\t\tread(%d, %x, %d) = %d\n", bf->fd, bf->buf,
			       bf->bufsiz, nread);
#endif
			if (nreread < bf->bufsiz - nread)
			{
				rts_error(VARLSTCNT(1) errno);
				return;
			}
			bf->remaining = nread;
		}
	}

	memcpy(buf, bf->bptr, nbytes);
	bf->bptr += nbytes;
	bf->remaining -= nbytes;
}


