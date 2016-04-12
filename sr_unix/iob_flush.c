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

/* iob_flush - buffered I/O with GT.M error signaling

   flush I/O buffer.  No action is taken if the last operation was not a
   write.

   void iob_flush(BFILE *bf)

   BFILE *bf:	file to flush

   Note:  It is assumed that the write buffer is full.  Exactly one
   	  record of size bf->bufsiz is written.

*/
#include "mdef.h"

#include "gtm_string.h"

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"

#include "error.h"
#include "iob.h"
#include "gtmio.h"

void iob_flush(BFILE *bf)
{
	ssize_t	nwritten, nrewritten;
	ssize_t	nbytes;
	int	rc;

	error_def(ERR_IOEOF);

	if (!bf->write_mode)
		return;

	if (bf->remaining == bf->bufsiz)	/* return if nothing to write */
		return;

	if (bf->remaining)			/* set remaining bytes to null */
	{
		memset(bf->bptr, 0, bf->remaining);
		/* bytes to transfer, rounded up to the nearest block */
		nbytes = (((bf->bptr - bf->buf) + bf->blksiz - 1) / bf->blksiz)
			* bf->blksiz;
	}
	else
		nbytes = bf->bufsiz;

	DOWRITERL(bf->fd, bf->buf, nbytes, nwritten);
#ifdef DEBUG_IOB
	PRINTF("iob_flush:\t\twrite(%d, %x, %d) = %d\n",bf->fd,bf->buf,nbytes,nwritten);
#endif
	bf->bptr = bf->buf;
	bf->remaining = bf->bufsiz;

	if (nwritten < nbytes)
	{
		CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
#ifdef SCO
		if (nwritten == -1)
		{
			if  (errno == ENXIO)
				nwritten = 0;
			else
			{
				rts_error(VARLSTCNT(1) errno);
				return;
			}
		}
#else
		if (nwritten == -1)
		{
			rts_error(VARLSTCNT(1) errno);
			return;
		}
#endif
		rts_error(VARLSTCNT(1) ERR_IOEOF);
		/* if we continued from here, assume that this is a magnetic
		   tape and we have loaded the next volume. Re-open and
		   finish the write operation.
		   */
		while (FD_INVALID == (bf->fd = OPEN3(bf->path,bf->oflag,bf->mode)))
			rts_error(VARLSTCNT(1) errno);
		DOWRITERL(bf->fd, bf->buf + nwritten, nbytes - nwritten, nrewritten);
#ifdef DEBUG_IOB
		PRINTF("iob_flush:\t\twrite(%d, %x, %d) = %d\n", bf->fd, bf->buf, nbytes, nwritten);
#endif
		if (nrewritten < nbytes - nwritten)
		{
			rts_error(VARLSTCNT(1) errno);
			return;
		}
	}
}

