/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_signal.h"
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
#include "gtmcrypt.h"
#include "t_retry.h"
#include "gdsdbver.h"
#include "min_max.h"
#include "gtmimagename.h"
#include "memcoherency.h"
#include "gdskill.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"         /* needed for tp.h */
#include "have_crit.h"
#include "tp.h"
#include "cdb_sc.h"
#include "mupip_reorg_encrypt.h"
#include "mu_reorg.h"
#include "gds_blk_upgrade_inline.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		dse_running, is_updhelper;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			mu_upgrade_in_prog;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			process_id;
GBLREF	uint4			mu_reorg_encrypt_in_prog;

int4	dsk_read (block_id blk, sm_uc_ptr_t buff, enum db_ver *ondsk_blkver, boolean_t blk_free)
{
	boolean_t		buff_is_modified_after_lseekread = FALSE, db_is_encrypted, fully_upgraded, use_new_key;
	block_id		blk_num, *blk_ptr, offset;
	char			*in, *out;
	enum db_ver		tmp_ondskblkver;
	int			bsiz, in_len, level, gtmcrypt_errno;
	int4			save_errno, size;
	intrpt_state_t		prev_intrpt_state;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		enc_save_buff, recBase;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	/* It is possible that an index block we read in from disk has a block_id that needs adjustment subsequent to enlargement
	 * of the master bit map. The database block scanning routines (gvcst_*search*.c) can deal with V6 or V7 index blocks,
	 * but only dsk_read does the offset adjustment Therefore we do not want to risk reading a potential pre-move index block
	 * directly into the cache and then adjusting it. Instead, we read it into a private buffer, upgrade it there and then
	 * copy it over to the cache. This uses the static variable read_reformat_buffer. We could have as well used the global
	 * variable "reformat_buffer" for this purpose. But that would then prevent dsk_reads and concurrent dsk_writes from
	 * proceeding. We don't want that loss of asynchronocity, hence we keep them separate. Note that while a lot of routines
	 * use "reformat_buffer" only this routine uses "read_reformat_buffer" which is a static rather than a GBLDEF.
	 */
	static sm_uc_ptr_t	read_reformat_buffer;
	unix_db_info		*udi;
	unsigned short		temp_ushort;
#	ifdef DEBUG
	unsigned int		effective_t_tries;
	boolean_t		killinprog;
	static int		in_dsk_read;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_errno = 0;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	assert(csd == cs_data);
	assert(NULL != cnl);
	assert(0 == in_dsk_read);	/* dsk_read should never be nested. the read_reformat_buffer logic below relies on this */
	DEBUG_ONLY(in_dsk_read++;)
	assert(GDSVCURR == GDSV7);	/* assert should fail if GDSVCURR changes */
	/* Note: Even in snapshots, only INTEG requires dsk_read to read FREE blocks. The assert below should be modified
	 * if we later introduce a scheme where we can figure out as to who started the snapshots and assert accordingly
	 */
	assert(!blk_free || SNAPSHOTS_IN_PROG(csa)); /* Only SNAPSHOTS require dsk_read to read a FREE block from the disk */
	udi = FILE_INFO(gv_cur_region);
	assert(csd == cs_data);
	size = csd->blk_size;
	tmp_ondskblkver = (enum db_ver)csd->desired_db_format;
	/* Cache csd->fully_upgraded once so that all uses work the same way. Repeatedly referencing csd->fully_upgraded could
	 * result in different values seen through-out the function resulting in incorrect operation. For example, the code does
	 * not allocate scratch space for the temporary pre-V7 formatted block which is needed later in the function. It is ok if
	 * the value of csd->fully_upgraded changes after we took a copy of it since we have a buffer locked for this particular
	 * block (in BG) so no concurrent process could be changing the format of this block. For MM, there is no such protection.
	 * We have 2 possibilities:
	 * - fully_upgraded is cached as FALSE but becomes TRUE before reading the block from disk. This only results in a little
	 *   extra work since the block on disk will already have been upgraded
	 * - fully_upgraded is cached as TRUE but becomes FALSE before reading the block from disk/mmap. There is no problem since
	 *   the process performs NO upgrade. Some later process will take the responsiblity for such an upgrade
	 */
	fully_upgraded = csd->fully_upgraded;
	assert(0 == (long)buff % SIZEOF(block_id));
	assert(NULL != cnl);
	INCR_GVSTATS_COUNTER(csa, cnl, n_dsk_read, 1);
	enc_save_buff = buff;
	/* The value of MUPIP_REORG_IN_PROG_LOCAL_DSK_READ indicates that this is a direct call from mupip_reorg_encrypt, operating
	 * on a local buffer.
	 */
	if (USES_ENCRYPTION(csd->is_encrypted) && (MUPIP_REORG_IN_PROG_LOCAL_DSK_READ != mu_reorg_encrypt_in_prog))
	{
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, buff);
		enc_save_buff = GDS_ANY_ENCRYPTGLOBUF(buff, csa);
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, enc_save_buff);
	}
	DB_LSEEKREAD(udi, udi->fd, (BLK_ZERO_OFF(csd->start_vbn) + ((off_t)blk * size)), enc_save_buff, size, save_errno);
	assert((0 == save_errno) || (-1 == save_errno));
	WBTEST_ASSIGN_ONLY(WBTEST_PREAD_SYSCALL_FAIL, save_errno, EIO);
	if ((enc_save_buff != buff) && (0 == save_errno))
	{
		assert(USES_ENCRYPTION(csd->is_encrypted) && (MUPIP_REORG_IN_PROG_LOCAL_DSK_READ != mu_reorg_encrypt_in_prog));
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_RECONFIG, prev_intrpt_state);
		db_is_encrypted = IS_ENCRYPTED(csd->is_encrypted);
		assert(NULL != csa->encr_ptr);
		assert(csa->encr_ptr->reorg_encrypt_cycle == cnl->reorg_encrypt_cycle);	/* caller should have ensured this */
		use_new_key = NEEDS_NEW_KEY(csd, ((blk_hdr_ptr_t)enc_save_buff)->tn);
		if (use_new_key || db_is_encrypted)
		{
			bsiz = (int)((blk_hdr_ptr_t)enc_save_buff)->bsiz;
			in_len = MIN(csd->blk_size, bsiz) - SIZEOF(blk_hdr);
			buff_is_modified_after_lseekread = TRUE;
			if (IS_BLK_ENCRYPTED(((blk_hdr_ptr_t)enc_save_buff)->levl, in_len))
			{	/* Due to concurrency conflicts, we are potentially reading a free block even though
				 * blk_free is FALSE. Go ahead and safely "decrypt" such a block, even though it contains no
				 * valid contents. We expect GTMCRYPT_DECRYPT to return success even if it is presented with
				 * garbage data.
				 */
				ASSERT_ENCRYPTION_INITIALIZED;
				memcpy(buff, enc_save_buff, SIZEOF(blk_hdr));
				in = (char *)(enc_save_buff + SIZEOF(blk_hdr));
				out = (char *)(buff + SIZEOF(blk_hdr));
				if (use_new_key)
				{
					GTMCRYPT_DECRYPT(csa, TRUE, csa->encr_key_handle2, in, in_len, out,
							enc_save_buff, SIZEOF(blk_hdr), gtmcrypt_errno);
					assert(0 == gtmcrypt_errno);
				} else
				{
					GTMCRYPT_DECRYPT(csa, csd->non_null_iv, csa->encr_key_handle, in, in_len, out,
							enc_save_buff, SIZEOF(blk_hdr), gtmcrypt_errno);
					assert(0 == gtmcrypt_errno);
				}
				save_errno = gtmcrypt_errno;
				DBG_RECORD_BLOCK_READ(csd, csa, cnl, process_id, blk, ((blk_hdr_ptr_t)enc_save_buff)->tn,
					1, use_new_key, enc_save_buff, buff, size, in_len);
			} else
			{
				memcpy(buff, enc_save_buff, size);
				DBG_RECORD_BLOCK_READ(csd, csa, cnl, process_id, blk, ((blk_hdr_ptr_t)enc_save_buff)->tn,
					2, use_new_key, enc_save_buff, buff, size, in_len);
			}
		} else
		{
			memcpy(buff, enc_save_buff, size);
			DBG_RECORD_BLOCK_READ(csd, csa, cnl, process_id, blk, ((blk_hdr_ptr_t)enc_save_buff)->tn,
				3, use_new_key, enc_save_buff, buff, size, 0);
		}
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_RECONFIG, prev_intrpt_state);
	}
	if (0 == save_errno)
	{	/* see if block needs to be converted to current version. This code block should be maintained in parallel
		 * with a similar section in mm_read */
		if ((GDSV6p == (tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver)) && (GDSMV70000 == csd->creation_mdb_ver))
		{       /* adjust for shift of GDSV7 id from 2 to 4 */
			buff_is_modified_after_lseekread = TRUE;
			tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver = GDSV7;
		}
		level = (int)((blk_hdr_ptr_t)buff)->levl;	/* Doing this here for the assert below */
		if (blk_free || (GDSV4 == tmp_ondskblkver))
		{	/* but might be uninitialed */
#			ifdef DEBUG
			if (!blk_free && !is_updhelper && !dse_running && !mu_reorg_encrypt_in_prog
					&& !mu_upgrade_in_prog)
				TREF(donot_commit) = DONOTCOMMIT_DSK_READ_EMPTY_BUT_NOT_FREE;	/* expected data, but got empty */
#			endif
			/* might not be correct, but any writer would correct it before it goes to a DB file */
			buff_is_modified_after_lseekread = TRUE;
			tmp_ondskblkver = csd->desired_db_format;
		}
#ifdef	DEBUG_UPGRADE
		/* The following assert can trip in regular operation when dealing with recycled blocks after the DB becomes
		 * fully upgraded after an MUPIP UPGRADE and MUPIP REORG -UPGRADE complete. Because this assert was instrumental
		 * in debugging the various states of blocks during the upgrade process, we leave it in
		 */
		else
			assert((GDSV7 == tmp_ondskblkver) || (GDSV6 == tmp_ondskblkver)	/* vanilla cases */
				|| (LCL_MAP_LEVL == level)	/* This assert does not apply to local bit maps, ever */
				|| (0 == level)	/* This assert cannot apply to level 0 blocks. Except for directory tree level 0
						 * blocks, all level zero data blocks can be ANY version from V6 to V7m depending
						 * on when they were created. */
				|| ((GDSV7m == tmp_ondskblkver) && IS_64_BLK_ID(buff)) 	/* block upgrade complete from V6 */
				|| (!fully_upgraded && (GDSV6p == tmp_ondskblkver)));	/* shuffled & adjusted but still 4byte ID */
#endif
		if (LCL_MAP_LEVL == level)
		{	/* local bit maps just get a version update because they were never counted in blk_to_upgrd */
			if ((GDSV7m > tmp_ondskblkver) && (GDSV7m == csd->desired_db_format))
			{	/* this in not necessary, but a nice touch */
				buff_is_modified_after_lseekread = TRUE;
				tmp_ondskblkver = GDSV7m;
			}
		} else if ((csd->offset) && (GDSV6p > tmp_ondskblkver) && level
				&& (MUPIP_REORG_IN_PROG_LOCAL_DSK_READ != mu_reorg_encrypt_in_prog))
		{	/* pre-V7 index block needing its offset adjusted */
			assert(MEMCMP_LIT(csd->label, GDS_LABEL) || (!fully_upgraded && (GDSV6p < csd->desired_db_format)));
			if (TRUE == blk_ptr_adjust(buff, csd->offset))
			{	/* Do not mark the buffer as changed if something went wrong. Let the caller deal with it */
				buff_is_modified_after_lseekread = TRUE;
				tmp_ondskblkver = GDSV6p;				/* 4 byte block_id with offset applied */
			}
		}
#ifdef	DEBUG_UPGRADE
		assert(!level || !fully_upgraded || (GDSV6p != tmp_ondskblkver) && (!MEMCMP_LIT(csd->label, GDS_LABEL)
			? ((GDSV7 == tmp_ondskblkver) || (GDSV7m == tmp_ondskblkver))
			: ((GDSV6 == tmp_ondskblkver) && !MEMCMP_LIT(csd->label, V6_GDS_LABEL))));
#endif
		assert((GDSV4 != tmp_ondskblkver) && (NULL != ondsk_blkver));	/* REORG encrypt does not pass ondsk_blkver */
		*ondsk_blkver = tmp_ondskblkver;
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
		((blk_hdr_ptr_t)buff)->bver = tmp_ondskblkver;
		SHM_WRITE_MEMORY_BARRIER;
	}
#	ifdef DEBUG
	in_dsk_read--;
	assert(0 == in_dsk_read);
	/* Expect t_tries to be 3 if we have crit. Exceptions: gvcst_redo_root_search (where t_tries is temporarily reset
	 * for the duration of the redo_root_search and so we should look at the real t_tries in redo_rootsrch_ctxt),
	 * gvcst_expand_free_subtree, REORG UPGRADE/DOWNGRADE, DSE (where we grab crit before doing the t_qread irrespective
	 * of t_tries), forward recovery (where we grab crit before doing everything), MUPIP TRIGGER -UPGRADE (where we
	 * grab crit before doing the entire ^#t upgrade TP transaction) OR bm_getfree (where we did a preemptive crit grab
	 * before doing a file extension).
	 */
	effective_t_tries = UNIX_ONLY( (TREF(in_gvcst_redo_root_search)) ? (TREF(redo_rootsrch_ctxt)).t_tries : ) t_tries;
	effective_t_tries = MAX(effective_t_tries, t_tries);
	killinprog = (NULL != ((dollar_tlevel) ? sgm_info_ptr->kip_csa : kip_csa));
	assert(dse_running || killinprog || jgbl.forw_phase_recovery || mu_upgrade_in_prog || mu_reorg_encrypt_in_prog
			GTMTRIG_ONLY(|| TREF(in_trigger_upgrade)) || TREF(in_bm_getfree_gdsfilext)
			|| (csa->now_crit != (CDB_STAGNATE > effective_t_tries)));
#	endif
	return save_errno;
}
