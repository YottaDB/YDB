/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include <iodef.h>
#include <psldef.h>
#include <lckdef.h>
#include <dvidef.h>
#include <rms.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "efn.h"
#include "jnl.h"
#include "iosp.h"
#include "vmsdtype.h"
#include "send_msg.h"
#include "dbfilop.h"
#include "disk_block_available.h"
#include "gtmmsg.h"
#include "iosb_disk.h"
#include "gtmio.h"
#include "error.h"

#define BLKS_PER_WRITE	64

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF  jnl_gbls_t              jgbl;
GBLREF	boolean_t		in_jnl_file_autoswitch;

error_def(ERR_DBFILERR);
error_def(ERR_DSKSPACEFLOW);
error_def(ERR_JNLNOCREATE);
error_def(ERR_JNLEXTEND);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLSPACELOW);
error_def(ERR_NEWJNLFILECREAT);
error_def(ERR_NOSPACEEXT);
error_def(ERR_JNLFILEXTERR);
error_def(ERR_JNLWRERR);
error_def(ERR_JNLRDERR);

static	const	unsigned short	zero_fid[3];

uint4 jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size)
{
	struct FAB		fab;
	struct XABFHC		xabfhc;
	struct NAM		nam;
	struct RAB		rab;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	GDS_INFO		*gds_info;
	jnl_create_info		jnl_info;
	jnl_file_header         header;
	file_control		*fc;
	boolean_t		need_extend;
	char			*buff, jnl_file_name[JNL_NAME_SIZE], prev_jnl_fn[MAX_FN_LEN];
	int			new_blocks, avail_blocks;
	uint4			new_alq, status;
	uint4			jnl_status = 0;
	unsigned short		fn_len;
	jnl_buffer_ptr_t     	jb;
	uint4			aligned_tot_jrec_size, count;
	io_status_block_disk	iosb;

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
	cnl = csa->nl;
	assert(csa == cs_addrs && csd == cs_data);
	assert(0 != memcmp(cnl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid)));
	assert(csa->now_crit || (csd->clustered && (CCST_CLOSED == cnl->ccp_state)));
	assert(&FILE_INFO(jpc->region)->s_addrs == csa);
	if (!JNL_ENABLED(csa) || (NOJNL == jpc->channel) || (JNL_FILE_SWITCHED(jpc)))
		GTMASSERT;	/* crit and messing with the journal file - how could it have vanished? */
	if (!csd->jnl_deq)
	{
		assert(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE) <= csd->jnl_alq);
		assert(csd->jnl_alq == csd->autoswitchlimit);
		new_blocks = csd->jnl_alq;
	} else
		/* May cause extension of  csd->jnl_deq * n blocks where n > 0 */
		new_blocks = ROUND_UP(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE), csd->jnl_deq);
	jb = jpc->jnl_buff;
	jpc->status = SS_NORMAL;
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
		xabfhc = cc$rms_xabfhc;
		nam = cc$rms_nam;
		fab = cc$rms_fab;
		fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
		fab.fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
		fab.fab$l_fop = FAB$M_CBT | FAB$M_NAM;
		fab.fab$l_xab = &xabfhc;
		fab.fab$l_nam = &nam;
		/* Get the file id from the header and open the journal file */
		memcpy(&nam.nam$t_dvi, cnl->jnl_file.jnl_file_id.dvi, SIZEOF(cnl->jnl_file.jnl_file_id.dvi));
		memcpy(&nam.nam$w_did, cnl->jnl_file.jnl_file_id.did, SIZEOF(cnl->jnl_file.jnl_file_id.did));
		memcpy(&nam.nam$w_fid, cnl->jnl_file.jnl_file_id.fid, SIZEOF(cnl->jnl_file.jnl_file_id.fid));
		if (SYSCALL_SUCCESS(jpc->status = sys$open(&fab)))
		{
			if (SYSCALL_SUCCESS((status = disk_block_available(jpc->channel, &avail_blocks))))
			{
				avail_blocks += xabfhc.xab$l_hbk - xabfhc.xab$l_ebk;
				if ((new_blocks * EXTEND_WARNING_FACTOR) > avail_blocks)
				{
					if (new_blocks > avail_blocks)
					{	/* if we cannot satisfy the requst, it is an error */
						send_msg(VARLSTCNT(6) ERR_NOSPACEEXT, 4, JNL_LEN_STR(csd),
							new_blocks, avail_blocks);
						new_blocks = 0;
						jpc->status = SS_NORMAL;
					} else
						send_msg(VARLSTCNT(5) ERR_DSKSPACEFLOW, 3, JNL_LEN_STR(csd),
							(avail_blocks - new_blocks));
				}
			} else
				send_msg(VARLSTCNT(5) ERR_JNLFILEXTERR, 2, JNL_LEN_STR(csd), status);
			fab.fab$w_deq = new_blocks;
			rab = cc$rms_rab;
			rab.rab$l_fab = &fab;
			rab.rab$b_mbc = BLKS_PER_WRITE;
			rab.rab$l_rop |= RAB$M_EOF;
			/* ensure current journal file size is well within autoswitchlimit */
			assert(csd->autoswitchlimit >= jb->filesize);
			if (new_blocks && (SYSCALL_SUCCESS(jpc->status = sys$connect(&rab))))
			{
				new_alq = jb->filesize + new_blocks;
				if (csd->autoswitchlimit < ((new_blocks * EXTEND_WARNING_FACTOR) + jb->filesize))
					send_msg(VARLSTCNT(5) ERR_JNLSPACELOW, 3, JNL_LEN_STR(csd),
						csd->autoswitchlimit - jb->filesize);
				/* switch journal file if the request cannot be satisfied */
				if (csd->autoswitchlimit < new_alq)
				{	/* Reached max, need to autoswitch */
					/* Ensure new journal file can hold the entire current
						transaction's journal record requirements */
					assert(csd->autoswitchlimit >= MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size));
					memset(&jnl_info, 0, SIZEOF(jnl_info));
					jnl_info.prev_jnl = &prev_jnl_fn[0];
					set_jnl_info(gv_cur_region, &jnl_info);
					assert(!jgbl.forw_phase_recovery || (NULL != jgbl.mur_pini_addr_reset_fnptr));
					assert(jgbl.forw_phase_recovery || (NULL == jgbl.mur_pini_addr_reset_fnptr));
					if (NULL != jgbl.mur_pini_addr_reset_fnptr)
						(*jgbl.mur_pini_addr_reset_fnptr)(csa);
					jnl_status = jnl_ensure_open();
					if (0 == jnl_status)
					{	/* flush the cache and jnl-buffer-contents to current journal file before
						 * switching to a new journal. Set a global variable in_jnl_file_autoswitch
						 * so jnl_write can know not to do the padding check. But because this is a global
						 * variable, we also need to make sure it is reset in case of errors during the
						 * autoswitch (or else calls to jnl_write after we are out of the autoswitch logic
						 * will continue to incorrectly not do the padding check. Hence a condition handler.
						 */
						in_jnl_file_autoswitch = TRUE;
						/* Also make sure time is not changed. This way if "jnl_write" as part of writing a
						 * journal record invokes jnl_file_extend, when the autoswitch is done and writing
						 * of the parent jnl_write resumes, we want it to continue with the same timestamp
						 * and not have to reset its time (non-trivial task) to reflect any changes since.
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
						wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
						jnl_file_close(gv_cur_region, TRUE, TRUE);
						REVERT;
						in_jnl_file_autoswitch = FALSE;
						jgbl.dont_reset_gbl_jrec_time = jgbl.save_dont_reset_gbl_jrec_time;
						DEBUG_ONLY(jgbl.save_dont_reset_gbl_jrec_time = FALSE);
						gds_info = FILE_INFO(gv_cur_region);
						assert(jnl_info.fn_len == gds_info->fab->fab$b_fns);
						assert(0 == memcmp(jnl_info.fn, gds_info->fab->fab$l_fna, jnl_info.fn_len));
						assert(!jnl_info.no_rename);
						assert(!jnl_info.no_prev_link);
						if (EXIT_NRM == cre_jnl_file(&jnl_info))
						{
							assert(!memcmp(csd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len));
							assert(csd->jnl_file_name[jnl_info.jnl_len] == '\0');
							assert(csd->jnl_file_len == jnl_info.jnl_len);
							assert(csd->jnl_buffer_size == jnl_info.buffer);
							assert(csd->jnl_alq == jnl_info.alloc);
							assert(csd->jnl_deq == jnl_info.extend);
							assert(csd->jnl_before_image == jnl_info.before_images);
							csd->jnl_checksum = jnl_info.checksum;
							csd->jnl_eovtn = csd->trans_hist.curr_tn;
							fc = gv_cur_region->dyn.addr->file_cntl;
							fc->op = FC_WRITE;	/* write needed for successful jnl_file_open() */
							fc->op_buff = (sm_uc_ptr_t)csd;
							fc->op_len = SGMNT_HDR_LEN;
							fc->op_pos = 1;
							send_msg(VARLSTCNT(4) ERR_NEWJNLFILECREAT, 2, JNL_LEN_STR(csd));
							/* Dequeue the journal lock on the current jnl generation */
							jpc->status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
							assert(SS$_NORMAL == jpc->status);
							jpc->jnllsb->lockid = 0;
							if ((SS_NORMAL != (status = set_jnl_file_close(SET_JNL_FILE_CLOSE_EXTEND)))
								|| (SS_NORMAL != (status = dbfilop(fc))))
							{
								send_msg(VARLSTCNT(7) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region),
											status, 0, gds_info->fab->fab$l_stv);
								jpc->status = ERR_JNLNOCREATE;
							} else
							{
								/* set_jnl_file_close() would have opened and closed
								 * the new journal. we therefore have to reopen it now.
								 * this code should be removed in V4.4. see comment
								 * in sr_vvms/set_jnl_file_close.c for details
								 * 	--- nars -- 2002/04/18
								 */
								jnl_status = jnl_ensure_open();	/* sets jpc->status */
								if (jnl_status != 0)
								{
									if (SS_NORMAL != jpc->status2)
									{
										if (!(IS_RMS_ERROR(jpc->status2) ||
												IS_SYSTEM_ERROR(jpc->status2)))
											rts_error(VARLSTCNT(9) jnl_status, 4,
												JNL_LEN_STR(csd),
												DB_LEN_STR(gv_cur_region),
												jpc->status, 0, jpc->status2);
										else
											rts_error(VARLSTCNT(8) jnl_status, 4,
												JNL_LEN_STR(csd),
												DB_LEN_STR(gv_cur_region),
												jpc->status, jpc->status2);
									}
									else
										rts_error(VARLSTCNT(7) jnl_status, 4,
											JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region),
											jpc->status);
								}
								assert(jb->filesize == csd->jnl_alq);
								aligned_tot_jrec_size =
									ALIGNED_ROUND_UP(MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size),
											csd->jnl_alq, csd->jnl_deq);
								if (aligned_tot_jrec_size > csd->jnl_alq)
								{	/* need to extend more than initial allocation
									 * in the new journal file to accommodate the
									 * current transaction.
									 */
									new_blocks = aligned_tot_jrec_size - csd->jnl_alq;
									assert(new_blocks);
									assert(0 == new_blocks % csd->jnl_deq);
									need_extend = TRUE;
								}
							}
						} else
						{
							send_msg(VARLSTCNT(4) ERR_JNLNOCREATE, 2, JNL_LEN_STR(csd));
							jpc->status = ERR_JNLNOCREATE;
						}
					} else
					{
						assert(FALSE);
						rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
					}
				} else
				{	/* nullify extension jnl blocks */
					buff = malloc(DISK_BLOCK_SIZE * BLKS_PER_WRITE);
					memset(buff, 0, DISK_BLOCK_SIZE * BLKS_PER_WRITE);
					rab.rab$w_rsz = DISK_BLOCK_SIZE * BLKS_PER_WRITE;
					rab.rab$l_rbf = buff;
					for (rab.rab$l_bkt = xabfhc.xab$l_ebk;
						(SYSCALL_SUCCESS(jpc->status)) && (rab.rab$l_bkt <= new_alq);
						rab.rab$l_bkt += BLKS_PER_WRITE)
					{
						if (rab.rab$l_bkt + BLKS_PER_WRITE > new_alq)
							rab.rab$w_rsz = (new_alq + 1 - rab.rab$l_bkt) * DISK_BLOCK_SIZE;
						jpc->status = sys$write(&rab);
						if (SYSCALL_ERROR(jpc->status))
						{
							assert(FALSE);
							rts_error(VARLSTCNT(6) ERR_JNLWRERR, 2, JNL_LEN_STR(csd), jpc->status,
								rab.rab$l_stv);
						}
					}
					jb->filesize = new_alq;	/* Actually this is virtual file size blocks */
					DO_FILE_READ(jpc->channel, 0, &header, REAL_JNL_HDR_LEN, jpc->status, jpc->status2);
					if (SYSCALL_ERROR(jpc->status))
					{
						assert(FALSE);
						rts_error(VARLSTCNT(5) ERR_JNLRDERR, 2, JNL_LEN_STR(csd), jpc->status);
					}
					assert((header.virtual_size + new_blocks) == new_alq);
					header.virtual_size = new_alq;
					JNL_DO_FILE_WRITE(NULL, NULL, jpc->channel, 0,
						&header, REAL_JNL_HDR_LEN, jpc->status, jpc->status2);
					if (SYSCALL_ERROR(jpc->status))
					{
						assert(FALSE);
						rts_error(VARLSTCNT(5) ERR_JNLWRERR, 2, JNL_LEN_STR(csd), jpc->status);
					}
					assert(!need_extend);	/* ensure we won't go through the for loop again */
					free(buff);
				}
			} else if (new_blocks)
			{
				assert(FALSE);
				jpc->status2 = rab.rab$l_stv;
			}
			if ((SYSCALL_ERROR(status = sys$close(&fab))) && new_blocks && (SYSCALL_SUCCESS(jpc->status)))
			{
				assert(FALSE);
				jpc->status = status;
			}
		} else
		{
			assert(FALSE);
			assert(SYSCALL_ERROR(jpc->status));	/* ensure we will follow the call to jnl_file_lost() below */
			jpc->status2 = fab.fab$l_stv;
		}
		if (SYSCALL_ERROR(jpc->status))
			break;		/* break on error */
	}
	if (0 >= new_blocks)
	{
		assert(FALSE);
		new_blocks = -1;
		jpc->status = ERR_JNLREADEOF;
	}
	if ((SYSCALL_SUCCESS(jpc->status)) && (new_blocks > 0))
	{
		if (csd->clustered)
			csa->ti->ccp_jnl_filesize = jb->filesize;	/* Need to pass new filesize to other machines */
		INCR_GVSTATS_COUNTER(csa, cnl, n_jnl_extends, 1);
		return EXIT_NRM;
	}
	/* Notify operator and terminate journalling */
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
