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

#ifndef DESC2MVAL_INCLUDED
#define DESC2MVAL_INCLUDED

void desc2mval(struct dsc$descriptor *src, mval *v);
void desc2mval_32(struct dsc$descriptor *src, mval *v);
void desc2mval_64(struct dsc64$descriptor *src, mval *v);

#endif /* DESC2MVAL_INCLUDED */
