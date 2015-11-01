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

#ifndef __MVALSUB_H__
#define __MVALSUB_H__

int mpath2mval(unsigned char *inbuff, int maxelem, mval *mvarray);
unsigned char *parse_subsc(unsigned char *inbuff, mval *v);
int msubpath2mval(unsigned char *inbuff, int maxelem, mval *mvarray);

#endif
