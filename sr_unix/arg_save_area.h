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

#ifndef ARG_SAVE_AREA_H
#define ARG_SAVE_AREA_H

/* size of the super stack frame required for argument passing */
#ifdef __sparc
#define ARG_SAVE_AREA_SIZE	96
#elif defined(__hppa)
#define ARG_SAVE_AREA_SIZE	1088
#elif defined(__i386)
/* no arg save area needed for x86 - but compiler cribs on 0-size arrays */
#define ARG_SAVE_AREA_SIZE	1
#elif defined(__alpha)
#define ARG_SAVE_AREA_SIZE	256*8	/* 256 quadwords */
#else
#define ARG_SAVE_AREA_SIZE	256*4
#endif

#endif
