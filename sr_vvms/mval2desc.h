/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MVAL2DESC_INCLUDED
#define MVAL2DESC_INCLUDED

void mval2desc(mval *v, struct dsc$descriptor *d);		/***type int added***/
void mval2desc_32(mval *v, struct dsc$descriptor *d);
void mval2desc_64(mval *v, struct dsc64$descriptor *d);

#endif /* MVAL2DESC_INCLUDED */
