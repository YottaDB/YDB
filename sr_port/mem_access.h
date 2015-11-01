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

#ifndef __MEM_ACCESS_H__
#define  __MEM_ACCESS_H__

void set_noaccess(unsigned char *na_page[], unsigned char *prvprt);
void reset_access(unsigned char *na_page[], unsigned char oldprt);

#endif
