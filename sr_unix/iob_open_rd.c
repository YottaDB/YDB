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

/* iob_open - buffered I/O with GT.M error signaling

   Open a file in read-only mode.

   BFILE *iob_open_rd(char *path, int bufsiz)

	path:		file to open
	blksiz:		size of I/O block
	blkfactor:	blocks to buffer for input (blocking factor)

	return value:
	File structure for use with other buffered I/O routines.

*/

#include "mdef.h"

#include <errno.h>
#include <sys/types.h>

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"

#include "iob.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

BFILE *iob_open_rd(path, blksiz, blkfactor)
    char *path;
    int blksiz;
    int blkfactor;
{
    int fd;
    BFILE *file;
#ifdef __MVS__
    int realfiletag;
    /* Need the ERR_BADTAG and ERR_TEXT  error_defs for the TAG_POLICY macro warning */
    error_def(ERR_TEXT);
    error_def(ERR_BADTAG);
#endif

    if (FD_INVALID == (fd = OPEN3(path,O_RDONLY,0)))
	return NULL;
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(path, realfiletag, TAG_BINARY, errno);
#endif


    file = malloc(SIZEOF(BFILE));
    file->fd = fd;
    file->path = malloc(strlen(path) + 1);
    strcpy(file->path, path);
    file->oflag = O_RDONLY;
    file->mode = 0;
    file->blksiz = blksiz;
    file->bufsiz = blksiz * blkfactor;
    file->buf = malloc(file->bufsiz);
    file->bptr = file->buf;
    file->remaining = 0;
    file->write_mode = FALSE;

    return file;
}






