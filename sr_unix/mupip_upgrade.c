/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mupip_upgrade.c: Main program to upgrade v3.x database files to v4.x database. */

/*
   Note:
        * This program uses two algorithms. One needs at least 2 * BLKS_PER_LMAP * blk_size buffer.
	* Algorithms are described later
   	* 'group' is defined as a group of BLKS_PER_LMAP blocks.
	* V3.x level should be "09". But this will also consider "01"
 */

#include "mdef.h"

#include "gtm_fcntl.h"
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtmio.h"
#include "iosp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v3_gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"           /* needed for gdsblkops.h */
#include "gdscc.h"            /* needed for CDB_CW_SET_SIZE macro in gdsblkops.h */
#include "min_max.h"          /* needed for gdsblkops.h and MIN,MAX usage in this module */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "cli.h"
#include "gtm_caseconv.h"
#include "is_file_identical.h"
#include "mupip_exit.h"
#include "mupip_upgrade.h"
#include "mu_upgrd.h"
#include "mu_upgrd_sig_init.h"
#include "mu_upgrd_header.h"
#include "mu_upgrd_adjust_blkptr.h"
#include "ipcrmid.h"
#include "mupip_upgrade_standalone.h"
#include "mutex.h"

#define NOFLUSH 0
#define FLUSH 1
#define GDS_ALT_V30 "01"

#define INVERT(x) x^1

GBLREF  seq_num         seq_num_one, seq_num_zero;

static void init_replication(sgmnt_data *new_head)
{
        /* initialize replication related fields */
        QWASSIGN(new_head->reg_seqno, seq_num_one);
        QWASSIGN(new_head->resync_seqno, seq_num_one);
        QWASSIGN(new_head->old_resync_seqno, seq_num_one);
        new_head->resync_tn = 1;
        new_head->repl_state = repl_closed;              /* default */
}

void mupip_upgrade(void)
{
	bool		rbno;
	unsigned char 	*upgrd_buff[2], upgrd_label[GDS_LABEL_SZ]="UPGRADE0304";
	char		fn[256];
	char		answer[4];
	unsigned short	fn_len;
	int4		fd, save_errno, old_hdr_size, new_hdr_size, status, bufsize, dsize, datasize[2];
	int4            old_hdr_size_vbn, new_hdr_size_vbn;
	int		fstat_res, sems;
	off_t 		last_full_grp_startoff, old_file_len, old_file_len2, read_off, write_off, old_start_vbn_off;
	block_id	last_full_grp_startblk;
	v3_sgmnt_data	old_head_data, *old_head;
	sgmnt_data	new_head_data, *new_head;
 	struct stat    	stat_buf;

	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUNOUPGRD);
	error_def(ERR_DBOPNERR);
	error_def(ERR_DBRDONLY);
	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBPREMATEOF);

	fn_len = sizeof(fn);
	if (!cli_get_str("FILE", fn, &fn_len))
		rts_error(VARLSTCNT(1) ERR_MUNODBNAME);
	if (!(mupip_upgrade_standalone(fn, &sems)))
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	if (-1 == (fd = OPEN(fn, O_RDWR)))
	{
		save_errno = errno;
		if (-1 != (fd = OPEN(fn, O_RDONLY)))
		{
			util_out_print("Cannot update read-only database.", FLUSH);
			rts_error(VARLSTCNT(5) ERR_DBRDONLY, 2, fn_len, fn, errno);
		}
		rts_error(VARLSTCNT(5) ERR_DBRDONLY, 2, fn_len, fn, save_errno);
	}
	/* Confirm before proceed */
	if (!mu_upgrd_confirmed(TRUE))
	{
		util_out_print("Upgrade canceled by user", FLUSH);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	util_out_print("Do not interrupt to avoid damage in database!!", FLUSH);
	util_out_print("Mupip upgrade started ...!/", FLUSH);
	mu_upgrd_sig_init();
	/* get file status */
	FSTAT_FILE(fd, &stat_buf, fstat_res);
	if (-1 == fstat_res)
		rts_error(VARLSTCNT(4) ERR_DBOPNERR, fn_len, fn, errno);
	old_file_len = stat_buf.st_size;

	/* Prepare v3.x file header buffer */
	old_hdr_size  = sizeof(*old_head);
	util_out_print("Old header size: !SL", FLUSH, old_hdr_size);
	old_head = &old_head_data;
	/* Prepare v4.x file header buffer */
	new_hdr_size = sizeof(*new_head);
	util_out_print("New header size: !SL", FLUSH, new_hdr_size);
	new_head = &new_head_data;
	memset(new_head, 0, new_hdr_size);
	old_hdr_size_vbn = DIVIDE_ROUND_UP(old_hdr_size, DISK_BLOCK_SIZE);
	new_hdr_size_vbn = DIVIDE_ROUND_UP(new_hdr_size, DISK_BLOCK_SIZE);
	/* READ header from V3.x file */
	LSEEKREAD(fd, 0, old_head, old_hdr_size, status);
	if (0 != status)
		if (-1 == status)
			rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
		else
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
	/* Check version */
	if (memcmp(&old_head->label[0], GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (memcmp(&old_head->label[0], GDS_LABEL, GDS_LABEL_SZ - 3))
		{	/* it is not a GTM database */
			close(fd);
			util_out_print("File !AD is not a GT.M database.!/", FLUSH, fn_len, fn);
			rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
		}else
		{	/* it is GTM database */
			/* is it not v3.x database?  */
			if (memcmp(&old_head->label[GDS_LABEL_SZ - 3],GDS_V30,2) !=0  &&
		   	    memcmp(&old_head->label[GDS_LABEL_SZ - 3],GDS_ALT_V30,2) != 0)
			{
				close(fd);
				util_out_print("File !AD has an unrecognized database version!/", FLUSH, fn_len, fn);
				rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
			}
		}
	}
	else
	{
		/* READ the header from file again as V4.x header */
                LSEEKREAD(fd, 0, new_head, new_hdr_size, status);
                if (0 != status)
			if (-1 != status)
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
			else
				rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
                if (QWNE(new_head->reg_seqno, seq_num_zero) ||
                    QWNE(new_head->resync_seqno, seq_num_zero) ||
                    (new_head->resync_tn != 0) ||
                    new_head->repl_state != repl_closed)
                {
                        util_out_print("!AD might already have been upgraded", FLUSH, fn_len, fn);
                        util_out_print("Do you wish to continue with the upgrade? [y/n] ", FLUSH);
                        SCANF("%s", answer);
                        if (answer[0] != 'y' && answer[0] != 'Y')
                        {
                                close(fd);
                                util_out_print("Upgrade canceled by user", FLUSH);
                                rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
                        }
                }
                init_replication(new_head);
		new_head->max_update_array_size = new_head->max_non_bm_update_array_size
                                       = ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(new_head), UPDATE_ARRAY_ALIGN_SIZE);
		new_head->max_update_array_size += ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
		new_head->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
		new_head->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
		new_head->mutex_spin_parms.mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
		new_head->semid = INVALID_SEMID;
		new_head->shmid = INVALID_SHMID;
                /* writing header */
                LSEEKWRITE(fd, 0, new_head, new_hdr_size, status);
                if (0 != status)
                        rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
                close(fd);
                util_out_print("File !AD successfully upgraded.!/", FLUSH, fn_len, fn);
                mupip_exit(SS_NORMAL);
	}
	if (old_head->createinprogress)
	{
		close(fd);
		util_out_print("Database creation in progress on file !AD.!/", FLUSH, fn_len, fn);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	if (old_head->file_corrupt)
	{
		close(fd);
		util_out_print("Database !AD is corrupted.!/", FLUSH, fn_len, fn);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	if ((((off_t)old_head->start_vbn - 1) * DISK_BLOCK_SIZE +
		(off_t)old_head->trans_hist.total_blks * old_head->blk_size + (off_t)DISK_BLOCK_SIZE != old_file_len) &&
	   (((off_t)old_head->start_vbn - 1) * DISK_BLOCK_SIZE +
		(off_t)old_head->trans_hist.total_blks * old_head->blk_size + (off_t)old_head->blk_size != old_file_len))
	{
		util_out_print("Incorrect start_vbn !SL or, block size !SL or, total blocks !SL",
			FLUSH, old_head->start_vbn, old_head->blk_size, old_head->trans_hist.total_blks);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	if (ROUND_DOWN(old_head->blk_size, DISK_BLOCK_SIZE) != old_head->blk_size)
	{
		util_out_print("Database block size !SL is not divisible by DISK_BLOCK_SIZE", FLUSH, old_head->blk_size);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
        mu_upgrd_header(old_head, new_head); /* Update header from v3.x to v4.x  */
        new_head->start_vbn = new_hdr_size_vbn + 1;
        new_head->free_space = 0;
        new_head->wc_blocked_t_end_hist.evnt_cnt = old_head->wc_blocked_t_end_hist2.evnt_cnt;
        new_head->wc_blocked_t_end_hist.evnt_tn  = old_head->wc_blocked_t_end_hist2.evnt_tn;
        init_replication(new_head);
	/*
	   A simple way of doing mupip upgrade is to move all the data after file header
	   towards the eof to make space and write down the header. This does not need any
	   computation or, change in data/index blocks.  This is a slow process because it
	   has mainly I/O, though no manipulation of database structures.  or index blocks.
	   This is okay for small database.

	   A time efficient way is to physically move second group of BLKS_PER_LMAP number of
	   blocks towards the eof and move first group of BLKS_PER_LMAP number of blocks in
	   place of 2nd group. Finally adjust all indices to point to the blocks correctly.
	   Also adjust master bit map.
	   (note: we cannot move first group from the beginning).

	   Detail algorithm as follows:
	   ---------------------------
	   // Allocate two buffers each to hold one group of data.
	   Read v3.x header and upgrade to v4.x
	   if file is big enough
	  	read group 1 in buff[0]
	   	read_off = offset of starting block of 2nd group.
	  	read group 2 in buff[1]
		write buff[0] at offset read_off

	        last_full_grp_startblk = points to the block where 2nd group of 512 blocks
			of old file will be written back.
		//Instead of searching for a free group we will write at the last full group
		//Say, we have 3000 blocks.  last_full_grp_startblk = 2048
		//			     (not 2560, because it is not full)
		//All data from that point upto eof will be read and saved in buffer
		read all remaining data from the point last_full_grp_startblk upto eof in buff[0]
		write buff[1] at the point of last_full_grp_startblk
		Now write buff[0] at the end of last write
		//Graphical Example: Each letter corresponds to a group of 512 blocks where first block
		// 	is local bit map. Last group U may be a group of less than 512 blocks.
		//      Extend towards right ------------------------------------------------------->
		//	old permutation:    [v3 head]  A B C D E F G H   I J K L M N O P  Q R S T U
		//	new permutation:    [v4 head ]   A C D E F G H   I J K L M N O P  Q R S T B U
		Finally traverse the tree and adjust block pointers
		Adjust master map
		write new v4.x header at bof

	    else
	    	bufsize = size of data for a group
		rbno = 0    // read buffer no. This switches between 0 and 1
		read_off = 0
		write_off = 0
		upgrd_buff[rbno] = new header
		data_size[rbno] = new header size
		rbno = INVERT(rbno);
		do while not eof
			data_size[rbno] = MIN(bufsize, remaining_data_size)
			Read data of size data_size[rbno] in upgrd_buff[rbno] and adjust read_off
			rbno = INVERT(rbno);
			Write upgrd_buff[rbno] of datasize[rbno] at write_off and increase write_off
		Enddo
		rbno = INVERT(rbno)
		Write upgrd_buff[rbno] of datasize[rbno] at write_off
	    endif
	*/
	bufsize = old_head->blk_size * BLKS_PER_LMAP;
	upgrd_buff[0] = (unsigned char*) malloc(bufsize);
	upgrd_buff[1] = (unsigned char*) malloc(bufsize);
	read_off =  old_start_vbn_off = (off_t)(old_head->start_vbn - 1) * DISK_BLOCK_SIZE; /* start vbn offset in bytes */
	last_full_grp_startblk = ROUND_DOWN(new_head->trans_hist.total_blks, BLKS_PER_LMAP); /* in block_id */
	last_full_grp_startoff = old_start_vbn_off + (off_t)last_full_grp_startblk * new_head->blk_size; /* offset in bytes */
	/* this calculation is used because some 3.2x database has GDS blk_size bytes at the end
	   instead of DISK_BLOCK_SIZE bytes. */
	old_file_len2 = old_head->start_vbn * DISK_BLOCK_SIZE + (off_t)old_head->blk_size * old_head->trans_hist.total_blks;
	/* Change Label to a temporary dummy value, so that other GTM process does not come
	while doing upgrade and corrupts database */
	LSEEKWRITE(fd, 0, upgrd_label, GDS_LABEL_SZ - 1, status);
	if (0 != status)
		rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
	if (old_head->trans_hist.total_blks > BLKS_PER_LMAP * 2)
	{
		/* recalculate start_vbn and free space, because there will be a gap after header */
		new_head->start_vbn = old_head->start_vbn + bufsize / DISK_BLOCK_SIZE;
		new_head->free_space = bufsize - (new_hdr_size_vbn - old_hdr_size_vbn) * DISK_BLOCK_SIZE;
		util_out_print("New starting VBN is: !SL !/", FLUSH, new_head->start_vbn);

		/* read 1st group of blocks */
		LSEEKREAD(fd, read_off, upgrd_buff[0], bufsize, status);
		if (0 != status)
			if (-1 == status)
				rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
			else
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		read_off = read_off + bufsize;
		/* read 2nd group of blocks */
		LSEEKREAD(fd, read_off, upgrd_buff[1], bufsize, status);
		if (0 != status)
			if (-1 == status)
				rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
			else
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		/* write 1st group of blocks in place of 2nd group */
		write_off = old_start_vbn_off + bufsize;
		LSEEKWRITE(fd, write_off, upgrd_buff[0], bufsize, status);
		if (0 != status)
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		/* read last group (# of blks <= BLKS_PER_LMAP) */
		dsize = old_file_len2 - last_full_grp_startoff;
		assert (dsize <= bufsize);
		LSEEKREAD(fd, last_full_grp_startoff, upgrd_buff[0], dsize, status);
		if (0 != status)
			if (-1 == status)
				rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
			else
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		/* write 2nd group of blocks */
		LSEEKWRITE(fd, last_full_grp_startoff, upgrd_buff[1], bufsize, status);
		if (0 != status)
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		 /* write last group read from old file */
		LSEEKWRITE(fd, last_full_grp_startoff + bufsize, upgrd_buff[0], dsize, status);
		if (0 != status)
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
		util_out_print("Please wait while index is being adjusted...!/", FLUSH);
		mu_upgrd_adjust_blkptr(1L, TRUE, new_head, fd, fn, fn_len);
		mu_upgrd_adjust_mm(new_head->master_map, DIVIDE_ROUND_UP(new_head->trans_hist.total_blks+1,BLKS_PER_LMAP));
		/* writing header */
		LSEEKWRITE(fd, 0, new_head, new_hdr_size, status);
		if (0 != status)
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
	}
	else /* very small database */
	{
		rbno = 0;
		write_off = 0;

		datasize[rbno] = new_hdr_size;
		memcpy(upgrd_buff[0], new_head, new_hdr_size);
		rbno = INVERT(rbno);

		while(read_off < old_file_len2)
		{
			datasize[rbno] = MIN (old_file_len2 - read_off, bufsize);
			LSEEKREAD(fd, read_off, upgrd_buff[rbno], datasize[rbno], status);
			if (0 != status)
				if (-1 == status)
					rts_error(VARLSTCNT(4) ERR_DBPREMATEOF, 2, fn_len, fn);
				else
					rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
			read_off += datasize[rbno];
			rbno = INVERT(rbno);


			LSEEKWRITE(fd, write_off, upgrd_buff[rbno], datasize[rbno], status);
			if (0 != status)
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
			write_off+= datasize[rbno];
		}
		rbno = INVERT(rbno);
		LSEEKWRITE(fd, write_off, upgrd_buff[rbno], datasize[rbno], status);
		if (0 != status)
			rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, fn_len, fn, status);
	} /* end if small database */
	free(upgrd_buff[0]);
	free(upgrd_buff[1]);
	close(fd);
	util_out_print("File !AD successfully upgraded.!/", FLUSH, fn_len, fn);
	if (0 != sem_rmid(sems))
	{
		util_out_print("Error with sem_rmid", TRUE);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	mupip_exit(SS_NORMAL);
}

