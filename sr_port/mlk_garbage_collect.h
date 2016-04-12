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

#ifndef MLK_GARBAGE_COLLECT_DEFINED

/* Declare parms for mlk_garbage_collect.c */

void mlk_garbage_collect(mlk_ctldata_ptr_t ctl,
			 uint4 size,
			 mlk_pvtblk *p);

#define MLK_GARBAGE_COLLECT_DEFINED

#endif
