/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#include "gtm_string.h"

#include <unistd.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include <sys/types.h>
#include "gtm_stat.h"
#include "iob.h"

BFILE *iob_open_rd(path, blksiz, blkfactor)
    char *path;
    int blksiz;
    int blkfactor;
{
    int fd;
    BFILE *file;

    if ((fd = OPEN3(path,O_RDONLY,0)) == -1)
	return NULL;

    file = malloc(sizeof(BFILE));
    file->fd = fd;
    file->path = malloc(strlen(path) + 1);
    memcpy(file->path, path, strlen(path) + 1);
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






