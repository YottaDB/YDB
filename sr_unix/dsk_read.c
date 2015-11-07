/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include "gtm_unistd.h"
#include "gtm_string.h"
#include <signal.h>
#include <errno.h>
#ifdef DEBUG
#include "gtm_stdio.h"
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "error.h"
#include "gtmio.h"
#include "gds_blk_upgrade.h"
#include "gdsbml.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "gdsdbver.h"
#include "min_max.h"
#include "gtmimagename.h"
#include "memcoherency.h"
#include "gdskill.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"         /* needed for tp.h */
#include "hashtab_int4.h"       /* needed for tp.h */
#include "tp.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_DYNUPGRDFAIL);

int4	dsk_read (block_id blk, sm_uc_ptr_t buff, enum db_ver *ondsk_blkver, boolean_t blk_free)
{
	unix_db_info		*udi;
	int4			size, save_errno;
	enum db_ver		tmp_ondskblkver;
	sm_uc_ptr_t		save_buff = NULL, enc_save_buff;
	boolean_t		fully_upgraded, buff_is_modified_after_lseekread;
	int			bsiz;
#	ifdef DEBUG
	unsigned int		effective_t_tries;
	boolean_t		killinprog;
	blk_hdr_ptr_t		blk_hdr_val;
	static int		in_dsk_read;
#	endif
	/* It is possible that the block that we read in from disk is a V4 format block.  The database block scanning routines
	 * (gvcst_*search*.c) that might be concurrently running currently assume all global buffers (particularly the block
	 * headers) are V5 format.  They are not robust enough to handle a V4 format block. Therefore we do not want to
	 * risk reading a potential V4 format block directly into the cache and then upgrading it. Instead we read it into
	 * a private buffer, upgrade it there and then copy it over to the cache in V5 format. This is the static variable
	 * read_reformat_buffer. We could have as well used the global variable "reformat_buffer" for this purpose. But
	 * that would then prevent dsk_reads and concurrent dsk_writes from proceeding. We dont want that loss of asynchronocity.
	 * Hence we keep them separate. Note that while "reformat_buffer" is used by a lot of routines, "read_reformat_buffer"
	 * is used only by this routine and hence is a static instead of a GBLDEF.
	 */
	static sm_uc_ptr_t	read_reformat_buffer;
	static int		read_reformat_buffer_len;
#	ifdef GTM_CRYPT
	int			in_len, gtmcrypt_errno;
	char			*in, *out;
	boolean_t		is_encrypted;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Note: Even in snapshots, only INTEG requires dsk_read to read FREE blocks. The assert below should be modified
	 * if we later introduce a scheme where we can figure out as to who started the snapshots and assert accordingly
	 */
	assert(!blk_free || SNAPSHOTS_IN_PROG(cs_addrs)); /* Only SNAPSHOTS require dsk_read to read a FREE block from the disk */
	assert(0 == in_dsk_read);	/* dsk_read should never be nested. the read_reformat_buffer logic below relies on this */
	DEBUG_ONLY(in_dsk_read++;)
	udi = (unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info);
	assert(cs_addrs->hdr == cs_data);
	size = cs_data->blk_size;
	assert (cs_data->acc_meth == dba_bg);
	/* Since cs_data->fully_upgraded is referenced more than once in this module (once explicitly and once in
	 * GDS_BLK_UPGRADE_IF_NEEDED macro used below), take a copy of it and use that so all usages see the same value.
	 * Not doing this, for example, can cause us to see the database as fully upgraded in the first check causing us
	 * not to allocate save_buff (a temporary buffer to hold a V4 format block) at all but later in the macro
	 * we might see the database as NOT fully upgraded so we might choose to call the function gds_blk_upgrade which
	 * does expect a temporary buffer to have been pre-allocated. It is ok if the value of cs_data->fully_upgraded
	 * changes after we took a copy of it since we have a buffer locked for this particular block (at least in BG)
	 * so no concurrent process could be changing the format of this block. For MM there might be an issue.
	 */
	fully_upgraded = cs_data->fully_upgraded;
	if (!blk_free && !fully_upgraded) /* No V4->V5 translations required if block is FREE */
	{
		buff_is_modified_after_lseekread = TRUE;
		save_buff = buff;
		if (size > read_reformat_buffer_len)
		{	/* do the same for the reformat_buffer used by dsk_read */
			assert(0 == fast_lock_count);	/* this is mainline (non-interrupt) code */
			++fast_lock_count;		/* No interrupts in free/malloc across this change */
			if (NULL != read_reformat_buffer)
				free(read_reformat_buffer);
			read_reformat_buffer = malloc(size);
			read_reformat_buffer_len = size;
			--fast_lock_count;
		}
		buff = read_reformat_buffer;
	} else
		buff_is_modified_after_lseekread = FALSE;
	assert(NULL != cs_addrs->nl);
	INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_dsk_read, 1);
	enc_save_buff = buff;
#	ifdef GTM_CRYPT
	is_encrypted = cs_data->is_encrypted;
	if (is_encrypted)
	{
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(cs_addrs, cs_data, buff);
		enc_save_buff = GDS_ANY_ENCRYPTGLOBUF(buff, cs_addrs);
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(cs_addrs, cs_data, enc_save_buff);
	}
#	endif
	LSEEKREAD(udi->fd,
		  (DISK_BLOCK_SIZE * (cs_data->start_vbn - 1) + (off_t)blk * size),
		  enc_save_buff,
		  size,
		  save_errno);
	assert((0 == save_errno) GTM_TRUNCATE_ONLY(|| (-1 == save_errno)));
	WBTEST_ASSIGN_ONLY(WBTEST_PREAD_SYSCALL_FAIL, save_errno, EIO);
#	ifdef GTM_CRYPT
	if (is_encrypted && (0 == save_errno))
	{
		bsiz = (int)((blk_hdr_ptr_t)enc_save_buff)->bsiz;
		in_len = MIN(cs_data->blk_size, bsiz) - SIZEOF(blk_hdr);
		buff_is_modified_after_lseekread = TRUE;
		/* Do not do encryption/decryption if block is FREE */
		if (!blk_free && (IS_BLK_ENCRYPTED(((blk_hdr_ptr_t)enc_save_buff)->levl, in_len)))
		{	/* Due to concurrency conflicts, we are potentially reading a free block even though blk_free is
			 * FALSE. Go ahead and safely "decrypt" such a block, even though it contains no valid contents.
			 * We expect GTMCRYPT_DECRYPT to return success even if it is presented with garbage data.
			 */
			ASSERT_ENCRYPTION_INITIALIZED;
			memcpy(buff, enc_save_buff, SIZEOF(blk_hdr));
			in = (char *)(enc_save_buff + SIZEOF(blk_hdr));
			out = (char *)(buff + SIZEOF(blk_hdr));
			GTMCRYPT_DECRYPT(cs_addrs, cs_addrs->encr_key_handle, in, in_len, out, gtmcrypt_errno);
			save_errno = gtmcrypt_errno;
		} else
			memcpy(buff, enc_save_buff, size);
	}
#	endif
	if (!blk_free && (0 == save_errno))
	{	/* See if block needs to be converted to current version. Assuming buffer is at least short aligned */
		assert(0 == (long)buff % 2);
		/* GDSV4 (0) version uses "buff->bver" as a block length so should always be > 0 when M code is running.
		 * The only exception is if the block has not been initialized (possible if it is BLK_FREE status in the
		 * bitmap). This is possible due to concurrency issues while traversing down the tree. But if we have
		 * crit on this region, we should not see these either.
		 */
		assert(!IS_MCODE_RUNNING || !cs_addrs->now_crit || ((blk_hdr_ptr_t)buff)->bver);
		/* Block must be converted to current version (if necessary) for use by internals.
		 * By definition, all blocks are converted from/to their on-disk version at the IO point.
		 */
		GDS_BLK_UPGRADE_IF_NEEDED(blk, buff, save_buff, cs_data, &tmp_ondskblkver, save_errno, fully_upgraded);
		DEBUG_DYNGRD_ONLY(
			if (GDSVCURR != tmp_ondskblkver)
				PRINTF("DSK_READ: Block %d being dynamically upgraded on read\n", blk);
		)
		assert((GDSV6 == tmp_ondskblkver) || (NULL != save_buff));	/* never read a V4 block directly into cache */
		if (NULL != ondsk_blkver)
			*ondsk_blkver = tmp_ondskblkver;
		/* a bitmap block should never be short of space for a dynamic upgrade. assert that. */
		assert((NULL == ondsk_blkver) || !IS_BITMAP_BLK(blk) || (ERR_DYNUPGRDFAIL != save_errno));
		/* If we didn't run gds_blk_upgrade which would move the block into the cache, we need to do
		 * it ourselves. Note that buff will be cleared by the GDS_BLK_UPGRADE_IF_NEEDED macro if
		 * buff and save_buff are different and gds_blk_upgrade was called.
		 */
		if ((NULL != save_buff) && (NULL != buff))	/* Buffer not moved by upgrade, we must move */
			memcpy(save_buff, buff, size);
	}
	if (buff_is_modified_after_lseekread)
	{	/* Normally the disk read (done in LSEEKREAD macro) would do the necessary write memory barrier to make the
		 * updated shared memory global buffer contents visible to all other processes as long as they see any later
		 * updates done to shared memory by the reader. But in case of a V4 -> V5 upgrade or reading of an encrypted
		 * block, the actual disk read would have happened into a different buffer. That would then be used as a
		 * source for the upgrade or decryption before placing the final contents in the input global buffer.
		 * We now need a write memory barrier before returning from this function to publish this shared memory
		 * update to other processes waiting on this read. Note: it is possible in rare cases (e.g. mupip reorg upgrade)
		 * that the input buffer is NOT a shared memory buffer in which case the write memory barrier is not necessary
		 * but it is not easily possible to identify that and we want to save if checks on the fast path and so do
		 * the memory barrier in all cases.
		 */
		SHM_WRITE_MEMORY_BARRIER;
	}
#	ifdef DEBUG
	in_dsk_read--;
	/* Expect t_tries to be 3 if we have crit. Exceptions: gvcst_redo_root_search (where t_tries is temporarily reset
	 * for the duration of the redo_root_search and so we should look at the real t_tries in redo_rootsrch_ctxt),
	 * gvcst_expand_free_subtree, REORG UPGRADE/DOWNGRADE, DSE (where we grab crit before doing the t_qread irrespective
	 * of t_tries), forward recovery (where we grab crit before doing everything)
	 */
	effective_t_tries = UNIX_ONLY( (TREF(in_gvcst_redo_root_search)) ? (TREF(redo_rootsrch_ctxt)).t_tries : ) t_tries;
	effective_t_tries = MAX(effective_t_tries, t_tries);
	killinprog = (NULL != ((dollar_tlevel) ? sgm_info_ptr->kip_csa : kip_csa));
	assert(dse_running || killinprog || jgbl.forw_phase_recovery || mu_reorg_upgrd_dwngrd_in_prog
			|| (cs_addrs->now_crit != (CDB_STAGNATE > effective_t_tries)));
	if (!blk_free && cs_addrs->now_crit && !dse_running && (0 == save_errno))
	{	/* Do basic checks on GDS block that was just read. Do it only if holding crit as we could read
		 * uninitialized blocks otherwise. Also DSE might read bad blocks even inside crit so skip checks.
		 */
		blk_hdr_val = (NULL != save_buff) ? (blk_hdr_ptr_t)save_buff : (blk_hdr_ptr_t)buff;
		GDS_BLK_HDR_CHECK(cs_data, blk_hdr_val, fully_upgraded);
	}
#	endif
	return save_errno;
}
