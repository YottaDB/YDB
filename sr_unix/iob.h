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

/* iob.h - definitions used with buffered I/O routines */

typedef struct {
    int fd;		/* descriptor for file */
    char *path;		/* filename */
    int oflag;		/* open flags (see the open system call) */
    int mode;		/* creation mode (see the open system call) */
    int blksiz;		/* minimum size of block */
    int bufsiz;		/* size of buffer (blksiz * blocking factor) */
    char *buf;		/* I/O buffer */
    char *bptr;		/* pointer to last character read/written + 1 */
    int remaining;	/* for read:  bytes remaining to be read from buffer.
			 * write:     remaining space in buffer */
    int write_mode;	/* last operation was a write */
} BFILE;

void iob_close(BFILE *bf);
void iob_flush(BFILE *bf);
BFILE *iob_open_rd(char *path, int blksiz, int blkfactor);
BFILE *iob_open_wt(char *path, int blksiz, int blkfactor);
void iob_read(BFILE *bf, char *buf, int nbytes);
void iob_write(BFILE *bf, char *buf, int nbytes);
