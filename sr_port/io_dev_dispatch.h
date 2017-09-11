/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

GBLDEF dev_dispatch_struct io_dev_dispatch[] =
{
	iotype(iott, iott, iott, nil),
	ionil_dev,			/* placeholder where mt once lived */
	iotype(iorm, iorm, iopi, iopi),
	iotype(ious, ious, ious, nil),
	ionil_dev,			/* placeholder where mb once lived */
	iotype(ionl, ionl, nil, nil),
	iotype(ioff, iorm, iopi, nil),
	iotype(iosocket, iosocket, iosocket, iosocket),
	iotype(iopi, iorm, iopi, iopi)
};
