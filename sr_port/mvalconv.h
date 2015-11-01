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

#ifndef __MVALCONV_H__
#define  __MVALCONV_H__

void i2smval(mval *v, uint4 i);
void i2usmval(mval *v, unsigned int i);
void i2mval(mval *v, int i);
void i2flt(mflt *v, int i);
double mval2double(mval *v);
int4 mval2i(mval *v);
uint4 mval2si(mval *v);
bool isint (mval *v);
int mpath2mval(char *inbuff, int maxelem, mval *mvarray);

#endif
