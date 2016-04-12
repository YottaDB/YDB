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

/*
 *  omi_buff_rsp.c ---
 *
 *	Buffer a transaction response.
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "omi.h"

void	omi_buff_rsp(omi_req_hdr *rhptr, omi_err_hdr *ehptr, omi_status status, char *bptr, int len)
{
	char	*tptr;

	tptr = bptr;

	/*  Fill in the header */
	OMI_VI_WRIT(OMI_SI_SIZ + OMI_RH_SIZ + len, tptr);
	OMI_SI_WRIT(OMI_RH_SIZ, tptr);
	/*  Set the error flags to the specified values */
	if (ehptr)
	{
		OMI_LI_WRIT(ehptr->class, tptr);
		OMI_SI_WRIT(ehptr->type, tptr);
		OMI_LI_WRIT(ehptr->modifier, tptr);
	} else
	{	/*  Otherwise set all to 0 (class, type, and modifier) */
		OMI_LI_WRIT(0, tptr);
		OMI_SI_WRIT(0, tptr);
		OMI_LI_WRIT(0, tptr);
	}
	/*  Server status */
	OMI_LI_WRIT(status, tptr);
	/*  Sequence number */
	OMI_LI_WRIT(rhptr->seq.value, tptr);
	/*  Reference ID */
	OMI_LI_WRIT(rhptr->ref.value, tptr);
	return;
}
