/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSFILEXT_H__
#define __GDSFILEXT_H__

#ifdef UNIX
uint4 gdsfilext(uint4 blocks, uint4 filesize, boolean_t trans_in_prog);
# define GDSFILEXT(BLOCKS, FILESIZE, TRANS_IN_PROG)	gdsfilext(BLOCKS, FILESIZE, TRANS_IN_PROG)
#else
uint4 gdsfilext(uint4 blocks, uint4 filesize);
# define GDSFILEXT(BLOCKS, FILESIZE, DUMMY)		gdsfilext(BLOCKS, FILESIZE)
#endif

#define TRANS_IN_PROG_FALSE	FALSE
#define TRANS_IN_PROG_TRUE	TRUE

#endif
