/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_io_dev_dispatch.h is a subset of this file which includes those needed only by MUPIP
 * so, if you need to make a change here, please keep the other one in sync.
 *
 * VMS can have addresses in literal constants while most Unix platforms cannot
 */

UNIX_ONLY(GBLDEF) VMS_ONLY(LITDEF) dev_dispatch_struct io_dev_dispatch[] =
{
#	ifdef UNIX
	iotype(iott, iott, iott, nil),
#	else
	iotype(iott, iott, nil, nil),
#	endif
	iotype(iomt, iomt, nil, nil),
#	ifdef UNIX
	iotype(iorm, iorm, iopi, iopi),
#	else
	iotype(iorm, iorm, nil, nil),
#	endif
	iotype(ious, ious, ious, nil),
#	ifdef UNIX
	ionil_dev,
#	else
	iotype(iomb, iomb, nil, nil),
#	endif
	iotype(ionl, ionl, nil, nil),
#	ifdef UNIX
	iotype(ioff, iorm, iopi, nil),
#	else
	iotype(ioff, iorm, nil, nil),
#	endif
	iotype(iosocket, iosocket, iosocket, iosocket)
#	ifdef UNIX
	,iotype(iopi, iorm, iopi, iopi)
#	endif
};
