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

/* iob_close - buffered I/O with GT.M error signaling

   void iob_close(BFILE *bf)

	bf:		file to close
*/
#include "mdef.h"

#include <errno.h>

#include "gtm_unistd.h"

#include "error.h"
#include "iob.h"
#include "gtmio.h"

void iob_close(BFILE *bf)
{
	int	rc;

	iob_flush(bf);
	CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
	if (-1 == rc)
		rts_error(VARLSTCNT(1) errno);
	free(bf->buf);
	free(bf->path);
	free(bf);
}
