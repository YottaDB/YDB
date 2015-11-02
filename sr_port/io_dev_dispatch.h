/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_io_dev_dispatch.h is a subset of this file which includes those needed only by MUPIP
 * so, if you need to make a change here, please keep the other one in sync.
 */

/* VMS can have addresses in literal constants while most Unix platforms cannot */

UNIX_ONLY(GBLDEF) VMS_ONLY(LITDEF) dev_dispatch_struct io_dev_dispatch[]=
{
	iotype(iott, iott, nil),
	iotype(iomt, iomt, nil),
	iotype(iorm, iorm, nil),
	iotype(ious, ious, ious),
	iotype(iomb, iomb, nil),
	iotype(ionl, ionl, nil),
	iotype(ioff, iorm, nil),
	iotype(iotcp, iotcp, iotcp),
	iotype(iosocket, iosocket, iosocket)
#ifdef UNIX
	,iotype(iopi, iorm, iopi)
#endif
};
