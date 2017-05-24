/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "db_write_eof_block.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"

/* #GTM_THREAD_SAFE : The below function db_write_eof_block is thread-safe */
int	db_write_eof_block(unix_db_info *udi, int fd, int blk_size, off_t offset, dio_buff_t *diobuff)
{
	int		status;
	char		*buff;
	sgmnt_addrs	*csa;
	boolean_t	free_needed;

	if ((NULL != udi) && udi->fd_opened_with_o_direct)
	{
		DIO_BUFF_EXPAND_IF_NEEDED(udi, blk_size, diobuff);
		buff = diobuff->aligned;
		free_needed = FALSE;
	} else
	{
		buff = (char *)malloc(blk_size);
		free_needed = TRUE;
	}
	memset(buff, 0, blk_size);
	if (NULL != udi)
	{
		csa = &udi->s_addrs;
		assert(fd == udi->fd);
		DB_LSEEKWRITE(csa, udi, udi->fn, fd, offset, buff, blk_size, status);
	} else
		DB_LSEEKWRITE(NULL, ((unix_db_info *)NULL), NULL, fd, offset, buff, blk_size, status);
	if (free_needed)
		free(buff);
	return status;
}
