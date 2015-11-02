/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "util.h"
#include "eintr_wrappers.h"
#include "db_snapshot.h"

static void ss_print_blk_details(block_id, blk_hdr_ptr_t);

static void ss_print_fil_hdr(snapshot_filhdr_ptr_t);

void	ss_anal_shdw_file(char	*filename, int flen)
{
	int			shdw_fd, bitmap_size, shadow_vbn, word, bit, num, tot_blks;
	int			status, db_blk_size;
	off_t			blk_offset;
	snapshot_filhdr_t	ss_filhdr;
	blk_hdr_ptr_t		bp = NULL;
	block_id		blkno;
	unsigned int		*bitmap_buffer = NULL;

	error_def(ERR_SSPREMATEOF);
	error_def(ERR_SSFILOPERR);
	OPENFILE(filename, O_RDONLY, shdw_fd);
	if (FD_INVALID == shdw_fd)
	{
		status = errno;
		gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("open"), flen, filename, status);
	}
	LSEEKREAD(shdw_fd, 0, ((sm_uc_ptr_t)&ss_filhdr), SNAPSHOT_HDR_SIZE, status);
	if (0 != status)
	{
		if (-1 != status)
		{
			gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("read"), flen, filename, status);
			return;
		} else
		{
			util_out_print("!/Premature EOF with !AD", TRUE, flen, filename);
			return;
		}
	}
	bitmap_size = (int)(ss_filhdr.ss_info.ss_shmsize - SNAPSHOT_HDR_SIZE);
	if (0 >= bitmap_size)
	{
		util_out_print("!/Incorrect snapshot file format", TRUE);
		return;
	}
	ss_print_fil_hdr(&ss_filhdr);
	bitmap_buffer = (unsigned int *)malloc(bitmap_size);
	memset(bitmap_buffer, 0, bitmap_size);
	LSEEKREAD(shdw_fd, SNAPSHOT_HDR_SIZE, (sm_uc_ptr_t)(bitmap_buffer), bitmap_size, status);
	if (0 != status)
	{
		if (-1 != status)
		{
			gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("read"), flen, filename, status);
			return;
		} else
		{
			util_out_print("!/Premature EOF with !AD", TRUE, flen, filename);
			return;
		}
	}
	tot_blks = ss_filhdr.ss_info.total_blks;
	shadow_vbn = ss_filhdr.ss_info.shadow_vbn;
	db_blk_size = ss_filhdr.ss_info.db_blk_size;
	assert(bitmap_size >= (tot_blks + 8 - 1) / 8);
	bp = malloc(db_blk_size);
	for (blkno = 0; blkno <= tot_blks; blkno++)
	{
		if (0 == blkno % BLKS_PER_WORD)
		{
			word = blkno / BLKS_PER_WORD;
			num = bitmap_buffer[word];
		}
		bit = blkno % BLKS_PER_WORD;
		if (num & (1 << bit))
		{
			blk_offset = ((DISK_BLOCK_SIZE * (off_t)(shadow_vbn - 1)) + ((off_t)(blkno) * db_blk_size));
			LSEEKREAD(shdw_fd, blk_offset, bp, db_blk_size, status);
			if (0 != status)
			{
				if (-1 != status)
				{
					gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("read"), flen, filename, status);
					return;
				} else
				{
					gtm_putmsg(VARLSTCNT(7) ERR_SSPREMATEOF, 5, blkno, db_blk_size, blk_offset, flen, filename);
					return;
				}
			}
			ss_print_blk_details(blkno, bp);
		}
	}
	if (NULL != bitmap_buffer)
		free(bitmap_buffer);
	if (NULL != bp)
		free(bp);
	return;
}

static void ss_print_fil_hdr(snapshot_filhdr_ptr_t ss_filhdr_ptr)
{
	util_out_print("----------------------------------------", TRUE);
	util_out_print("Snapshot Initiator PID   :!UL", TRUE, ss_filhdr_ptr->ss_info.ss_pid);
	util_out_print("Snapshot TN              :0x!16@XQ", TRUE, &ss_filhdr_ptr->ss_info.snapshot_tn);
	util_out_print("DB Block Size            :!UL", TRUE, ss_filhdr_ptr->ss_info.db_blk_size);
	util_out_print("Free Blocks              :!UL", TRUE, ss_filhdr_ptr->ss_info.free_blks);
	util_out_print("Total Blocks             :!UL", TRUE, ss_filhdr_ptr->ss_info.total_blks);
	util_out_print("Shadow File              :!AD", TRUE, ss_filhdr_ptr->shadow_file_len, ss_filhdr_ptr->ss_info.shadow_file);
	util_out_print("Shadow Start VBN         :!UL", TRUE, ss_filhdr_ptr->ss_info.shadow_vbn);
	util_out_print("Shadow Shared Mem Size   :!UL", TRUE, ss_filhdr_ptr->ss_info.ss_shmsize);
	util_out_print("----------------------------------------", TRUE);
	return;
}
static void ss_print_blk_details(block_id blk, blk_hdr_ptr_t bp)
{
	util_out_print("--------------------------", TRUE);
	util_out_print("Block Number     : !UL", TRUE, blk);
	util_out_print("Block TN         : 0x!16@XQ", TRUE, &bp->tn);
	util_out_print("Block Level      : !UL", TRUE, bp->levl);
	util_out_print("Block Size       : !UL", TRUE, bp->bsiz);
	util_out_print("--------------------------", TRUE);
	return;
}
