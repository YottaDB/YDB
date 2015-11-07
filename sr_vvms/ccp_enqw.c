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

#include "error.h"
#include "locks.h"

/*
 *	ccp_enqw is identical to sys$enqw, except that it causes the CCPSIGCONT
 *	message to be issued to OPCOM if sys$enqw returns an unsuccessful status.
 */

uint4 ccp_enqw(
	unsigned int	efn,
	unsigned int	lkmode,
	lock_sb		*lksb,
	unsigned int	flags,
	void		*resnam,
	unsigned int	parid,
	void		*astadr,
	unsigned int	astprm,
	void		*blkast,
	unsigned int	acmode,
	unsigned int	nullarg)
{
	uint4	status;
	error_def(ERR_CCPSIGCONT);

	status = sys$enqw(efn, lkmode, lksb, flags, resnam, parid, astadr, astprm, blkast, acmode, nullarg);
	if (SUCCESS & status)
		status = (uint4)lksb->cond;
	if ((ERROR | SEVERE) & status)
		rts_error(VARLSTCNT(4) ERR_CCPSIGCONT, 1, caller_id(), status);
	return status;
}
