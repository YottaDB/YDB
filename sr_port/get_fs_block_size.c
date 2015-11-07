/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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

#ifndef VMS
#include "gtm_statvfs.h"
#endif

uint4	get_fs_block_size(int fd)
{
#	ifndef VMS
	struct statvfs		bufvfs;
	int			status;
#	endif
	uint4			gtm_fs_block_size;
	unsigned long		sys_fs_block_size;

#	if defined(__vms)
	sys_fs_block_size = DISK_BLOCK_SIZE;
#	else
	FSTATVFS(fd, &bufvfs, status);
	assert(-1 != status);
	assert(SIZEOF(sys_fs_block_size) == SIZEOF(bufvfs.f_frsize));
	/* If fstatvfs call fails, we dont know what the underlying filesystem size is.
	 * We found some NFS implementations return bufvfs.f.frsize values that are inappropriate.
	 * Instead of erroring out at this point, we assume a safe value (4K) and continue as much as we can.
	 */
	sys_fs_block_size = ((-1 == status) || (MAX_IO_BLOCK_SIZE < bufvfs.f_frsize)
		|| (DISK_BLOCK_SIZE > bufvfs.f_frsize)) ? 4096 : bufvfs.f_frsize;
#	endif
	/* Fit file system block size in a 4-byte unsigned integer as that is the size in jnl_buffer.
	 * Assert that we never get a block size > what can be held in a 4-byte unsigned integer.
	 */
	gtm_fs_block_size = (uint4)sys_fs_block_size;
	assert(gtm_fs_block_size == sys_fs_block_size);
	assert(MAX_IO_BLOCK_SIZE >= gtm_fs_block_size);
	assert(MAX_IO_BLOCK_SIZE % gtm_fs_block_size == 0);
	return gtm_fs_block_size;
}
