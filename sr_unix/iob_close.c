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

/* iob_close - buffered I/O with GT.M error signaling

   void iob_close(BFILE *bf)

	bf:		file to close
*/
#include <errno.h>
#include "mdef.h"
#include "gtm_unistd.h"
#include "error.h"
#include "iob.h"

void iob_close(BFILE *bf)
{
    iob_flush(bf);
    if (close(bf->fd) == -1)
	rts_error(VARLSTCNT(1) errno);

    free(bf->buf);
    free(bf->path);
    free(bf);
}
