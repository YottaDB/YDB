/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WAIT_FOR_DISK_SPACE_H__
#define __WAIT_FOR_DISK_SPACE_H__

void wait_for_disk_space(sgmnt_addrs *csa, char *fn, int fd, off_t offset, char *buf, size_t count, int *save_errno);

#endif
