/****************************************************************
 *								*
 * Copyright (c) 2012-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef WAIT_FOR_DISK_SPACE_H_INCLUDED
#define WAIT_FOR_DISK_SPACE_H_INCLUDED

void wait_for_disk_space(sgmnt_addrs *csa, char *fn, int fd, off_t offset, char *buf, size_t count, int *save_errno);

#endif
