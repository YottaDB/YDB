/****************************************************************
 *								*
 * Copyright (c) 2016-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

/* #GTM_THREAD_SAFE : The below function db_write_eof_block is thread-safe */
int	db_write_eof_block(unix_db_info *udi, int fd, int blk_size, off_t offset, dio_buff_t *diobuff)
{
	int		status;
	char		*buff;
	sgmnt_addrs	*csa;
	boolean_t	free_needed;

	if ((NULL != udi) && udi->fd_opened_with_o_direct)
	{
		assert(diobuff);
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
#		ifdef DEBUG
		block_id	save_ydb_skip_bml_num;

		/* It is possible we write an EOF block to the offset corresponding to bitmap block BLKS_PER_LMAP. In that case,
		 * we want to disable the "ydb_skip_bml_num" related assert in DB_LSEEKWRITE. We do that by resetting
		 * "ydb_skip_bml_num" to 0 in that case.
		 */
		save_ydb_skip_bml_num = ydb_skip_bml_num;
		if (offset == (BLK_ZERO_OFF(udi->s_addrs.hdr->start_vbn) + (BLKS_PER_LMAP * udi->s_addrs.hdr->blk_size)))
			ydb_skip_bml_num = 0;
#		endif
		DB_LSEEKWRITE(csa, udi, udi->fn, fd, offset, buff, blk_size, status);
#		ifdef DEBUG
		ydb_skip_bml_num = save_ydb_skip_bml_num;	/* restore "ydb_skip_bml_num" in case it got reset */
#		endif
	} else
		DB_LSEEKWRITE(NULL, ((unix_db_info *)NULL), NULL, fd, offset, buff, blk_size, status);
	if (free_needed)
		free(buff);
	return status;
}
