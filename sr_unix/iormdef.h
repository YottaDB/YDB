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

#define DEF_RM_WIDTH	32767
#define DEF_RM_LENGTH	66

#define RM_NOOP		0
#define RM_WRITE	1
#define RM_READ		2

/* ***************************************************** */
/* *********** structure for RMS driver **************** */
/* ***************************************************** */

typedef struct
{
	bool		fixed;
	bool		noread;
	bool		stream;
	bool		fifo;
	unsigned char	lastop;
	int		fildes;
	FILE		*filstr;
}d_rm_struct;	/*  rms		*/

#ifdef __MVS__
#define NATIVE_NL	0x15		/* EBCDIC */
#else
#define NATIVE_NL	'\n'		/* ASCII */
#endif
