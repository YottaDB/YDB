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

#include "mdef.h"
#include "mem_access.h"

#define	PRT$C_NA	0

/* Set a region of memory to be inaccessable */
void set_noaccess(unsigned char *na_page[2], unsigned char *prvprt)
/*
unsigned char *na_page[2];		array of addresses: the low and high addresses to be protected
unsigned char *prvprt;			A place to save the previous protection, should the caller later
					   wish to restore the protection
*/
{
	unsigned	status;
	status = sys$setprt (na_page, 0, 0, PRT$C_NA, prvprt);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	return;
}

/* Return memory protection to the state of affairs which existed prior to a call to set_noaccess */
void reset_access(unsigned char *na_page[2], unsigned char oldprt)
/*
unsigned char *na_page[2];		array of addresses: the low and high addresses to be protected
unsigned char oldprt;			A place to save the previous protection, should the caller later
					   wish to restore the protection
*/
{
	unsigned	status;
	status = sys$setprt (na_page, 0, 0, oldprt, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	return;
}
