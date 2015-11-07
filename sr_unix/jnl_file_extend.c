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

#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_statvfs.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "iosp.h"
#include "jnl.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "disk_block_available.h"
#include "wcs_flu.h"
#include "anticipatory_freeze.h"
#include "error.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	boolean_t		in_jnl_file_autoswitch;

error_def(ERR_JNLEXTEND);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLSPACELOW);
error_def(ERR_NEWJNLFILECREAT);
error_def(ERR_DSKSPACEFLOW);
error_def(ERR_JNLFILEXTERR);
error_def(ERR_DBFILERR);
error_def(ERR_NOSPACEEXT);
error_def(ERR_JNLRDERR);
error_def(ERR_JNLWRERR);
error_def(ERR_JNLNOCREATE);
error_def(ERR_PREMATEOF);

uint4 jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size)
{
	file_control		*fc;
	boolean_t		need_extend;
	jnl_buffer_ptr_t     	jb;
	jnl_create_info 	jnl_info;
	jnl_file_header		*header;
	unsigned char		hdr_buff[REAL_JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	uint4			new_alq;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char			prev_jnl_fn[JNL_NAME_SIZE];
	uint4			jnl_status = 0, status;
	int			new_blocks, warn_blocks, result;
	gtm_uint64_t		avail_blocks;
	uint4			aligned_tot_jrec_size, count;
	uint4			jnl_fs_block_size, read_write_size;
	DCL_THREADGBL_ACCESS;

	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		GTMASSERT;
	}
	csd = csa->hdr;
	assert(csa == cs_addrs && csd == cs_data);
	assert(csa->now_crit || (csd->clustered && (CCST_CLOSED == csa->nl->ccp_state)));
	assert(&FILE_INFO(jpc->region)->s_addrs == csa);
	assert(csa->jnl_state == csd->jnl_state);
	assertpro(JNL_ENABLED(csa) && (NOJNL != jpc->channel) && (!JNL_FILE_SWITCHED(jpc)));
		/* crit and messing with the journal file - how could it have vanished? */
	if (!csd->jnl_deq || (csd->jnl_alq + csd->jnl_deq > csd->autoswitchlimit))
	{
		assert(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE) <= csd->jnl_alq);
		assert(csd->jnl_alq == csd->autoswitchlimit);
		new_blocks = csd->jnl_alq;
	} else
		/* May cause extension of  csd->jnl_deq * n blocks where n > 0 */
		new_blocks = ROUND_UP(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE), csd->jnl_deq);
	jpc->status = SS_NORMAL;
	jb = jpc->jnl_buff;
	assert(0 <= new_blocks);
	DEBUG_ONLY(count = 0);
	for (need_extend = (0 != new_blocks); need_extend; )
	{
		DEBUG_ONLY(count++);
		/* usually we will do the loop just once where we do the file extension.
		 * rarely we might need to do an autoswitch instead after which again rarely
		 * 	we might need to do an extension on the new journal to fit in the transaction's	journal requirements.
		 * therefore we should do this loop a maximum of twice. hence the assert below.
		 */
		assert(count <= 2);
		need_extend = FALSE;
		if (SS_NORMAL == (status = disk_block_available(jpc->channel, &avail_blocks, TRUE)))
		{
			warn_blocks = (csd->jnl_alq + csd->jnl_deq > csd->autoswitchlimit)
					? ((csd->jnl_deq > csd->autoswitchlimit) ? csd->jnl_deq : csd->autoswitchlimit)
					: new_blocks;

			if ((warn_blocks * EXTEND_WARNING_FACTOR) > avail_blocks)
			{
				if (new_blocks > avail_blocks)
				{	/* If we cannot satisfy the request, it is an error, unless the anticipatory freeze
					 * scheme is in effect in which case, we will assume space is available even if
					 * it is not and go ahead with writes to the disk. If the writes fail with ENOSPC
					 * we will freeze the instance and wait for space to become available and keep
					 * retrying the writes. Therefore, we make the NOSPACEEXT a warning in this case.
					 */
					SETUP_THREADGBL_ACCESS;
					if (!ANTICIPATORY_FREEZE_ENABLED(csa))
					{
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_NOSPACEEXT, 4,
								JNL_LEN_STR(csd), new_blocks, avail_blocks);
						new_blocks = 0;
						jpc->status = SS_NORMAL;
						break;
					} else
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) MAKE_MSG_WARNING(ERR_NOSPACEEXT), 4,
								JNL_LEN_STR(csd), new_blocks, avail_blocks);
				} else
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DSKSPACEFLOW, 3, JNL_LEN_STR(csd),
							(avail_blocks - warn_blocks));
			}
		} else
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLFILEXTERR, 2, JNL_LEN_STR(csd), status);
		new_alq = jb->filesize + new_blocks;
		/* ensure current journal file size is well within autoswitchlimit --> design constraint */
		assert(csd->autoswitchlimit >= jb->filesize);
		if (csd->autoswitchlimit < (jb->filesize + (EXTEND_WARNING_FACTOR * new_blocks)))	/* close to max */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLSPACELOW, 3, JNL_LEN_STR(csd),
					csd->autoswitchlimit - jb->filesize);
		if (csd->autoswitchlimit < new_alq)
		{	/* Reached max, need to autoswitch */
			/* Ensure new journal file can hold the entire current transaction's journal record requirements */
			assert(csd->autoswitchlimit >= MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size));
			memset(&jnl_info, 0, SIZEOF(jnl_info));
			jnl_info.prev_jnl = &prev_jnl_fn[0];
			set_jnl_info(gv_cur_region, &jnl_info);
			assert(JNL_ENABLED(csa) && (NOJNL != jpc->channel) && !(JNL_FILE_SWITCHED(jpc)));
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{	/* flush the cache and jnl-buffer-contents to current journal file before
				 * switching to a new journal. Set a global variable in_jnl_file_autoswitch
				 * so jnl_write can know not to do the padding check. But because this is a global
				 * variable, we also need to make sure it is reset in case of errors during the
				 * autoswitch (or else calls to jnl_write after we are out of the autoswitch logic
				 * will continue to incorrectly not do the padding check. Hence a condition handler.
				 */
				assert(!in_jnl_file_autoswitch);
				in_jnl_file_autoswitch = TRUE;
				/* Also make sure time is not changed. This way if "jnl_write" as part of writing a
				 * journal record invokes jnl_file_extend, when the autoswitch is done and writing
				 * of the parent jnl_write resumes, we want it to continue with the same timestamp
				 * and not have to reset its time (non-trivial task) to reflect any changes since then.
				 */
				assert(!jgbl.save_dont_reset_gbl_jrec_time);
				jgbl.save_dont_reset_gbl_jrec_time = jgbl.dont_reset_gbl_jrec_time;
				jgbl.dont_reset_gbl_jrec_time = TRUE;
				/* Establish a condition handler so we reset a few global variables that have
				 * temporarily been modified in case of errors inside wcs_flu/jnl_file_close.
				 */
				ESTABLISH_RET(jnl_file_autoswitch_ch, EXIT_ERR);
				/* It is possible we still have not written a PINI record in this journal file
				 * (e.g. mupip extend saw the need to do jnl_file_extend inside jnl_write while
				 * trying to write a PINI record). Write a PINI record in that case before closing
				 * the journal file that way the EOF record will have a non-zero pini_addr.
				 */
				if (0 == jpc->pini_addr)
					jnl_put_jrt_pini(csa);
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SPEEDUP_NOBEFORE);
				jnl_file_close(gv_cur_region, TRUE, TRUE);
				REVERT;
				in_jnl_file_autoswitch = FALSE;
				jgbl.dont_reset_gbl_jrec_time = jgbl.save_dont_reset_gbl_jrec_time;
				DEBUG_ONLY(jgbl.save_dont_reset_gbl_jrec_time = FALSE);
				assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
				csd = cs_data;	/* In MM, wcs_flu() can remap an extended DB, so reset csd to be sure */
			} else
			{
				if (SS_NORMAL != jpc->status)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
							DB_LEN_STR(gv_cur_region), jpc->status);
				else
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
							DB_LEN_STR(gv_cur_region));
			}
			assert(!jgbl.forw_phase_recovery || (NULL != jgbl.mur_pini_addr_reset_fnptr));
			assert(jgbl.forw_phase_recovery || (NULL == jgbl.mur_pini_addr_reset_fnptr));
			if (NULL != jgbl.mur_pini_addr_reset_fnptr)
				(*jgbl.mur_pini_addr_reset_fnptr)(csa);
			assert(!jnl_info.no_rename);
			assert(!jnl_info.no_prev_link);
			if (EXIT_NRM == cre_jnl_file(&jnl_info))
			{
				assert(0 == memcmp(csd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len));
				assert(csd->jnl_file_name[jnl_info.jnl_len] == '\0');
				assert(csd->jnl_file_len == jnl_info.jnl_len);
				assert(csd->jnl_buffer_size == jnl_info.buffer);
				assert(csd->jnl_alq == jnl_info.alloc);
				assert(csd->jnl_deq == jnl_info.extend);
				assert(csd->jnl_before_image == jnl_info.before_images);
				csd->jnl_checksum = jnl_info.checksum;
				csd->jnl_eovtn = csd->trans_hist.curr_tn;
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_NEWJNLFILECREAT, 2, JNL_LEN_STR(csd));
				fc = gv_cur_region->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (sm_uc_ptr_t)csd;
				fc->op_len = SGMNT_HDR_LEN;
				fc->op_pos = 1;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
				assert(JNL_ENABLED(csa));
				/* call jnl_ensure_open instead of jnl_file_open to make sure jpc->pini_addr is set to 0 */
				jnl_status = jnl_ensure_open();	/* sets jpc->status */
				if (0 != jnl_status)
				{
					if (jpc->status)
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
								DB_LEN_STR(gv_cur_region), jpc->status);
					else
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
								DB_LEN_STR(gv_cur_region));
				}
				assert(jb->filesize == csd->jnl_alq);
				if (csd->jnl_alq + csd->jnl_deq <= csd->autoswitchlimit)
				{
					aligned_tot_jrec_size = ALIGNED_ROUND_UP(MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size),
											csd->jnl_alq, csd->jnl_deq);
					if (aligned_tot_jrec_size > csd->jnl_alq)
					{	/* need to extend more than initial allocation in the new journal file
						 * to accommodate the current transaction.
						 */
						new_blocks = aligned_tot_jrec_size - csd->jnl_alq;
						assert(new_blocks);
						assert(0 == new_blocks % csd->jnl_deq);
						need_extend = TRUE;
					}
				}
			} else
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_JNLNOCREATE, 2, JNL_LEN_STR(csd));
				jpc->status = ERR_JNLNOCREATE;
				new_blocks = -1;
			}
		} else
		{
			assert(!need_extend);	/* ensure we won't go through the for loop again */
			/* Virtually extend currently used journal file */
			jnl_fs_block_size = jb->fs_block_size;
			header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_buff, jnl_fs_block_size));
			read_write_size = ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size);
			assert((unsigned char *)header + read_write_size <= ARRAYTOP(hdr_buff));
			DO_FILE_READ(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
			if (SS_NORMAL != jpc->status)
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLRDERR, 2, JNL_LEN_STR(csd), jpc->status);
			}
			assert((header->virtual_size + new_blocks) == new_alq);
			jb->filesize = new_alq;	/* Actually this is virtual file size blocks */
			header->virtual_size = new_alq;
			JNL_DO_FILE_WRITE(csa, csd->jnl_file_name, jpc->channel, 0,
					header, read_write_size, jpc->status, jpc->status2);
			if (SS_NORMAL != jpc->status)
			{
				assert(WBTEST_RECOVER_ENOSPC == gtm_white_box_test_case_number);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLWRERR, 2, JNL_LEN_STR(csd), jpc->status);
			}
		}
		if (0 >= new_blocks)
			break;
	}
	if (0 < new_blocks)
	{
		INCR_GVSTATS_COUNTER(csa, csa->nl, n_jnl_extends, 1);
		return EXIT_NRM;
	}
	jpc->status = ERR_JNLREADEOF;
	jnl_file_lost(jpc, ERR_JNLEXTEND);
       	return EXIT_ERR;
}

CONDITION_HANDLER(jnl_file_autoswitch_ch)
{
	START_CH;
	assert(in_jnl_file_autoswitch);
	in_jnl_file_autoswitch = FALSE;
	jgbl.dont_reset_gbl_jrec_time = jgbl.save_dont_reset_gbl_jrec_time;
	DEBUG_ONLY(jgbl.save_dont_reset_gbl_jrec_time = FALSE);
	NEXTCH;
}
