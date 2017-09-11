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

/* io_dev_dispatch.h is a superset of this file which includes those needed by GT.M
 * so, if you need to make a change here, please keep the other one in sync.
 *
 * Following definitions have a pattern that most of the routines follow. Only exceptions is:
 *      1. ioff_open() is an extra routine
 */
GBLDEF dev_dispatch_struct io_dev_dispatch_mupip[] =
{
	iotype(iott, iott, iott, nil),
	ionil_dev,
	iotype(iorm, iorm, iopi, iopi),
	ionil_dev,
	ionil_dev,
	ionil_dev,
	iotype(ioff, iorm, iopi, nil),
	ionil_dev,
	ionil_dev
};
