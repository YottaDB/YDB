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

#ifndef DB_WRITE_EOF_BLOCK_H_INCLUDED
#define DB_WRITE_EOF_BLOCK_H_INCLUDED

/* Reduction in free blocks after truncating from a to b total blocks: a = old_total (larger), b = new_total */
# define DELTA_FREE_BLOCKS(a, b)	((a - b) - (DIVIDE_ROUND_UP(a, BLKS_PER_LMAP) - DIVIDE_ROUND_UP(b, BLKS_PER_LMAP)))

int	db_write_eof_block(unix_db_info *udi, int fd, int blk_size, off_t offset, dio_buff_t *diobuff);

#endif
