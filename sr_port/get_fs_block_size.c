/****************************************************************
 *								*
 * Copyright (c) 2010-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "have_crit.h"
#include "eintr_wrappers.h"
#include "get_fs_block_size.h"
#include "gtm_statvfs.h"
#ifdef DEBUG
#include "is_fstype_nfs.h"
#endif

OS_PAGE_SIZE_DECLARE

uint4	get_fs_block_size(int fd)
{
	struct statvfs		bufvfs;
	int			status;
	uint4			gtm_fs_block_size;
	unsigned long		sys_fs_block_size;

	FSTATVFS(fd, &bufvfs, status);
	assert(-1 != status);
	assert(SIZEOF(sys_fs_block_size) == SIZEOF(bufvfs.f_frsize));
	/* If fstatvfs call fails, we don't know what the underlying filesystem size is.
	 * We found some NFS implementations return bufvfs.f.frsize values that are inappropriate.
	 * Instead of erroring out at this point, we assume a safe value (the OS page size) and continue as much as we can.
	 */
	assert(DISK_BLOCK_SIZE <= bufvfs.f_frsize);
	assert((OS_PAGE_SIZE >= bufvfs.f_frsize) || is_fstype_nfs(fd));
	sys_fs_block_size = ((-1 == status) || (OS_PAGE_SIZE < bufvfs.f_frsize)
			|| (DISK_BLOCK_SIZE > bufvfs.f_frsize)) ? OS_PAGE_SIZE : bufvfs.f_frsize;
	/* Fit file system block size in a 4-byte unsigned integer as that is the size in jnl_buffer.
	 * Assert that we never get a block size > what can be held in a 4-byte unsigned integer.
	 */
	gtm_fs_block_size = (uint4)sys_fs_block_size;
	assert(gtm_fs_block_size == sys_fs_block_size);
	assert(MAX_IO_BLOCK_SIZE >= gtm_fs_block_size);
	assert(MAX_IO_BLOCK_SIZE % gtm_fs_block_size == 0);
	return gtm_fs_block_size;
}
