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

/* iob_write - buffered I/O with GT.M error signaling

   void iob_write(BFILE *bf, void *buf, int nbytes)

   BFILE *bf:	file to write
   buf:		buffer to place data read;
   nbytes:	bytes to write.

*/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_stdio.h"
#include <errno.h>
#include "error.h"
#include "iob.h"

void iob_write(BFILE *bf, char *buf, int nbytes)
{
    int4 nwrite;
    error_def(ERR_IOEOF);

    if (!bf->write_mode)
    {
	bf->write_mode = TRUE;
	bf->remaining = bf->bufsiz;
	bf->bptr = bf->buf;
    }

#ifdef DEBUG_IOB
    PRINTF("iob_write:\tiob_write(%x, %x, %d), bf->remaining = %d\n",
	   bf, buf, nbytes, bf->remaining);
#endif

    while (nbytes > bf->remaining)
    {
	/* fill buffer */
	memcpy(bf->bptr, buf, bf->remaining);
	nbytes -= bf->remaining;
	buf += bf->remaining;
	bf->remaining = 0;

	/* empty */
	iob_flush(bf);
    }

    memcpy(bf->bptr, buf, nbytes);
    bf->bptr += nbytes;
    bf->remaining -= nbytes;
}


