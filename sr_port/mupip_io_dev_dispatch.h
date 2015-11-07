/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
****************************************************************/

/* io_dev_dispatch.h is a superset of this file which includes those needed by GT.M
 * so, if you need to make a change here, please keep the other one in sync.
 *
 * Following definitions have a pattern that most of the routines follow. Only exceptions is:
 *      1. ioff_open() is an extra routine
 *
 * VMS can have addresses in literal constants while most Unix platforms cannot
 */
UNIX_ONLY(GBLDEF) VMS_ONLY(LITDEF) dev_dispatch_struct io_dev_dispatch_mupip[] =
{
#	ifdef UNIX
	iotype(iott, iott, iott),
#	else
	ionil_dev,
#	endif
	ionil_dev,
#	ifdef UNIX
	iotype(iorm, iorm, iopi),
#	else
	iotype(iorm, iorm, nil),
#	endif
	ionil_dev,
	ionil_dev,
	ionil_dev,
#	ifdef UNIX
	iotype(ioff, iorm, iopi),
#	else
	iotype(ioff, iorm, nil),
#	endif
	ionil_dev,
	ionil_dev
#	ifdef UNIX
	,ionil_dev
#	endif
};
