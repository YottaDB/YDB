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

#ifndef GVCMZ_INCLUDED
#define GVCMZ_INCLUDED

void gvcmz_error(char code, uint4 status);
void gvcmz_neterr(char *error);
void gvcmz_bunch(mval *v);
void gvcmz_clrlkreq(void);
void gvcmz_doop(char query_code, char reply_code, mval *v);
void gvcmz_zflush(void);

#endif /* GVCMZ_INCLUDED */
