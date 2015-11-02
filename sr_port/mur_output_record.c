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

#include "gtm_string.h"
#include "gtm_time.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "copy.h"
#include "util.h"
#include "op.h"
#include "tp_set_sgm.h"
#include "cache.h"
#include "gtmmsg.h"
#include "jnl_typedef.h"
#include "jnl_get_checksum.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "send_msg.h"
#include "svnames.h"		/* for SV_ZTWORMHOLE */
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"
#endif
/* Include prototypes */
#include "t_qread.h"
#include "t_end.h"
#include "dbfilop.h"
#include "gvcst_blk_build.h"
#include "jnl_write.h"
#include "op_tcommit.h"
#include "gvcst_jrt_null.h"	/* for gvcst_jrt_null prototype */
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF  mur_opt_struct  	mur_options;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF  sgmnt_data_ptr_t 	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		write_after_image;
GBLREF	jnl_fence_control	jnl_fence_ctl; /* Needed to set the token, optimize jnl_write_logical for recover */
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF  jnl_process_vector	*prc_vec;
GBLREF 	mur_gbls_t		murgbl;

error_def(ERR_JNLBADRECFMT);

/* This routine is called only for recover and rollback (that is, mur_options.update).
 * It applies the set/kill/zkill, tcom, inctn, and aimg records during forward processing.
 * Some fields like jnl_seqno, rec_seqno and prefix.time are saved here from original journal files.
 * Later jnl_write routines copies them to journal records instead of generating them like the runtime system */
uint4	mur_output_record(reg_ctl_list *rctl)
{
	mval			mv;
	jnl_record		*rec;
	char			*val_ptr;
	int			strm_num;
	uint4			dummy;
	off_jnl_t		pini_addr;
	jnl_string		*keystr;
	enum jnl_record_type 	rectype;
	uint4			jnl_status, status;
	pini_list_struct	*plst;
	boolean_t		jnl_enabled, was_crit;
	struct_jrec_null	null_record;
	gd_region		*reg;
	seq_num			strm_seqno;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_ctl_list		*jctl;
	jnl_format_buffer	*ztworm_jfb;
#	ifdef GTM_CRYPT
	blk_hdr_ptr_t		aimg_blk_ptr;
	int			in_len, gtmcrypt_errno ;
#	endif

	assert(mur_options.update);
	rec = rctl->mur_desc->jnlrec;
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	switch (rectype)
	{
		case JRT_ALIGN:
		case JRT_EOF:
		case JRT_EPOCH:
		case JRT_PBLK:
		case JRT_PINI:
		case JRT_TRUNC:
			return SS_NORMAL;
			break;
		default:
			break;
	}
	jgbl.gbl_jrec_time = rec->prefix.time;
	pini_addr = rec->prefix.pini_addr;
	reg = rctl->gd;
	jctl = rctl->jctl;
	assert(jctl->reg_ctl == rctl);
	assert(gv_cur_region == reg);
	csa = rctl->csa;
	assert(cs_addrs == csa);
	csd = csa->hdr;
	assert(cs_data == csd);
	jnl_enabled = JNL_ENABLED(csa);
	if (jnl_enabled)
	{
		status = mur_get_pini(jctl, pini_addr, &plst);
		if (SS_NORMAL != status)
			return status;
		prc_vec = &plst->jpv;
		csa->jnl->pini_addr = plst->new_pini_addr;
		rctl->mur_plst = plst;
		if (mur_options.rollback && IS_REPLICATED(rectype))
		{
			jgbl.mur_jrec_seqno = GET_JNL_SEQNO(rec);
			if (jgbl.mur_jrec_seqno >= murgbl.consist_jnl_seqno)
			{
				assert(murgbl.losttn_seqno >= (jgbl.mur_jrec_seqno + 1));
				murgbl.consist_jnl_seqno = jgbl.mur_jrec_seqno + 1;
			}
			UNIX_ONLY(
				jgbl.mur_jrec_strm_seqno = GET_STRM_SEQNO(rec);
				if (strm_seqno = jgbl.mur_jrec_strm_seqno)	/* caution: assignment */
				{	/* maintain csd->strm_reg_seqno */
					strm_num = GET_STRM_INDEX(strm_seqno);
					strm_seqno = GET_STRM_SEQ60(strm_seqno);
					assert(csd->strm_reg_seqno[strm_num] <= (strm_seqno + 1));
					csd->strm_reg_seqno[strm_num] = strm_seqno + 1;
				}
			)
		}
	}
	if (IS_SET_KILL_ZKILL_ZTRIG(rectype))
	{	/* TP and non-TP has same format */
		keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
		if (jnl_enabled)
		{
			MUR_SET_JNL_FENCE_CTL_TOKEN(rec->jrec_set_kill.token_seq.token, rctl);
			jnl_fence_ctl.strm_seqno = rec->jrec_set_kill.strm_seqno;
			jgbl.tp_ztp_jnl_upd_num = rec->jrec_set_kill.update_num;
			DEBUG_ONLY(jgbl.max_tp_ztp_jnl_upd_num = MAX(jgbl.max_tp_ztp_jnl_upd_num, jgbl.tp_ztp_jnl_upd_num);)
			jgbl.mur_jrec_nodeflags = keystr->nodeflags;
		}
		if (IS_FENCED(rectype))
		{	/* Even for FENCE_NONE we apply fences. Otherwise an [F/G/T/U]UPD becomes UPD etc. */
			/* op_tstart is called in "mur_forward_play_cur_jrec" already */
			if (IS_FUPD(rectype))
			{
				jnl_fence_ctl.level = 1;
				if (jnl_enabled)
				{
					jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
					csa->next_fenced = NULL;
				}
			} else if (IS_GUPD(rectype))
			{
				jnl_fence_ctl.level = 1;
				if (jnl_enabled)
				{
					jnl_fence_ctl.fence_list = csa;
					csa->next_fenced = JNL_FENCE_LIST_END;
				}
			} else if (IS_TP(rectype))
				tp_set_sgm();
		}
#		ifdef GTM_TRIGGER
		/* Check if ^#t and if so need to increment trigger cycle in file header. Note that the below 'if' check could cause
		 * csd->db_trigger_cycle to be incremented even for the region that actually did NOT get any trigger updates. This
		 * is because some of the ^#t subscripts (like ^#t(#TNAME)) go to the DEFAULT region. So, even though a trigger was
		 * loaded only for ^a (corresponding to AREG), csd->db_trigger_cycle will be incremented for DEFAULT region as well.
		 * To avoid this, the below check should be modified to set csa->incr_db_trigger_cycle only if the ^#t subscript
		 * does not begin with '#' (similar to what is done in UPD_GV_BIND_NAME_APPROPRIATE). However, since journal
		 * recovery operates in standalone mode, the db_trigger_cycle increment to DEFAULT region should be okay since it
		 * will NOT cause any restarts
		 */
		if (IS_GVKEY_HASHT_GBLNAME(keystr->length, keystr->text))
		{
			assert(cs_addrs == csa);
			csa->incr_db_trigger_cycle = TRUE;
		}
#		endif
		if (IS_SET(rectype))
		{
			val_ptr = &keystr->text[keystr->length];
			GET_MSTR_LEN(mv.str.len, val_ptr);
			mv.str.addr = val_ptr + SIZEOF(mstr_len_t);
			mv.mvtype = MV_STR;
			op_gvput(&mv);
		} else if (IS_KILL(rectype))
		{
			if (IS_TP(rectype))
				tp_set_sgm();
			op_gvkill();
#		ifdef GTM_TRIGGER
		} else if (IS_ZTRIG(rectype))
		{
			if (IS_TP(rectype))
				tp_set_sgm();
			op_ztrigger();
#		endif
		} else
		{
			assert(IS_ZKILL(rectype));
			if (IS_TP(rectype))
				tp_set_sgm();
			op_gvzwithdraw();
		}
		if (IS_ZTP(rectype))
		{	/* Even for FENCE_NONE we apply fences. Otherwise an FUPD/GUPD becomes UPD etc. */
			assert(jnl_enabled || (JNL_FENCE_LIST_END == jnl_fence_ctl.fence_list && NULL == csa->next_fenced));
			jnl_fence_ctl.level = 0;
			if (jnl_enabled)
			{
				jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
				csa->next_fenced = NULL;
			}
		}
		return SS_NORMAL;
	}
	switch(rectype)
	{
#	ifdef GTM_TRIGGER
	case JRT_TZTWORM:
	case JRT_UZTWORM:
		if (jnl_enabled)
		{	/* Format the ZTWORM journal record */
			assert(dollar_tlevel);	/* op_tstart should already have been done by mur_forward */
			MUR_SET_JNL_FENCE_CTL_TOKEN(rec->jrec_ztworm.token_seq.token, rctl);
			jnl_fence_ctl.strm_seqno = rec->jrec_ztworm.strm_seqno;
			jgbl.tp_ztp_jnl_upd_num = rec->jrec_ztworm.update_num;
			DEBUG_ONLY(jgbl.max_tp_ztp_jnl_upd_num = MAX(jgbl.max_tp_ztp_jnl_upd_num, jgbl.tp_ztp_jnl_upd_num);)
			jgbl.mur_jrec_nodeflags = 0;
			keystr = (jnl_string *)&rec->jrec_ztworm.ztworm_str;
			mv.str.addr = &keystr->text[0];
			mv.str.len = keystr->length;
			mv.mvtype = MV_STR;
			ztworm_jfb = jnl_format(JNL_ZTWORM, NULL, &mv, 0);
			assert(NULL != ztworm_jfb);
		}
		break;
#	endif
	case JRT_TCOM:
		/* If forw_multi is non-NULL, it means we are playing a multi-region TP transaction as ONE TP transaction.
		 * While we will come to mur_output_record for each TCOM record, we will do the actual commit AFTER we
		 * are done playing the journal records of ALL regions involved in the TP.
		 */
		if (NULL == rctl->forw_multi)
		{	/* Even for FENCE_NONE we apply fences. Otherwise a TUPD/UUPD becomes UPD etc. */
			if (jnl_enabled)
			{
				MUR_SET_JNL_FENCE_CTL_TOKEN(rec->jrec_tcom.token_seq.token, rctl);
				jnl_fence_ctl.strm_seqno = rec->jrec_tcom.strm_seqno;
				jgbl.mur_jrec_participants = rec->jrec_tcom.num_participants;
				memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE);
			}
			assert(dollar_tlevel);
			op_tcommit();
		} else
			MUR_DBG_SET_LAST_PROCESSED_JNL_SEQNO(rec->jrec_tcom.token_seq.token, rctl);
		break;
	case JRT_ZTCOM:
		/* Even for FENCE_NONE we apply fences. Otherwise an FUPD/GUPD becomes UPD etc. */
		if (jnl_enabled)
		{
			MUR_SET_JNL_FENCE_CTL_TOKEN(rec->jrec_ztcom.token, rctl);
			jnl_fence_ctl.strm_seqno = 0;	/* strm_seqno is only for replication & ZTCOM does not work with replic */
			jgbl.mur_jrec_participants = rec->jrec_ztcom.participants;
		}
		jnl_fence_ctl.level = 1;
		if (jnl_enabled)
		{
			jnl_fence_ctl.fence_list = csa;
			csa->next_fenced = JNL_FENCE_LIST_END;
		}
		op_ztcommit(1);
		assert(jnl_enabled || (JNL_FENCE_LIST_END == jnl_fence_ctl.fence_list && NULL == csa->next_fenced));
		jnl_fence_ctl.level = 0;
		jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		csa->next_fenced = NULL;
		break;
	case JRT_INCTN:
		assert(csa == (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs);
		assert(csd == csa->hdr);
		if (mur_options.forward)
		{
			assert(rec->jrec_inctn.prefix.tn == csd->trans_hist.curr_tn || mur_options.notncheck);
			if (FALSE == ((was_crit = csa->now_crit)))
				grab_crit(reg);
			CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
			csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
			INCREMENT_CURR_TN(csd);
			if (!was_crit)
				rel_crit(reg);
		}
		break;
	case JRT_AIMG:
		assert(csa == (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs);
		assert(csd == csa->hdr);
		assert(!dollar_tlevel);
		if (!mur_options.apply_after_image)
			return SS_NORMAL;
		write_after_image = TRUE;
#		ifdef GTM_CRYPT
		aimg_blk_ptr = (blk_hdr_ptr_t)&rec->jrec_aimg.blk_contents[0];
		if (csd->is_encrypted)
		{
			assert((aimg_blk_ptr->bsiz <= csd->blk_size) && (aimg_blk_ptr->bsiz >= SIZEOF(blk_hdr)));
			in_len = MIN(csd->blk_size, aimg_blk_ptr->bsiz) - SIZEOF(blk_hdr);
			ASSERT_ENCRYPTION_INITIALIZED;
			if (IS_BLK_ENCRYPTED(aimg_blk_ptr->levl, in_len))
			{
				GTMCRYPT_DECRYPT(csa, jctl->encr_key_handle, (char *)(aimg_blk_ptr + 1), in_len, NULL,
							gtmcrypt_errno)
				if (0 != gtmcrypt_errno)
					GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, jctl->jnl_fn_len, jctl->jnl_fn);
			}
		}
#		endif
		mur_put_aimg_rec(rec);
		write_after_image = FALSE;
		break;
	case JRT_PFIN:
		if (jnl_enabled)
		{
			if (FALSE == ((was_crit = rctl->csa->now_crit)))
				grab_crit(reg);
			/* MUPIP RECOVER should be the only one updating the database so journal state not expected to changes */
			assert(JNL_ENABLED(csd));
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				assert(plst->new_pini_addr == csa->jnl->pini_addr);
				if (0 != csa->jnl->pini_addr)
					jnl_put_jrt_pfin(rctl->csa);
			} else
			{
				if (SS_NORMAL != csa->jnl->status)
					rts_error(VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), csa->jnl->status);
				else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
			}
			if (!was_crit)
				rel_crit(reg);
		}
		break;
	case JRT_NULL:
		assert(cs_addrs == rctl->csa);
		if (jnl_enabled)
		{
			MUR_SET_JNL_FENCE_CTL_TOKEN(rec->jrec_null.jnl_seqno, rctl);
			jnl_fence_ctl.strm_seqno = rec->jrec_null.strm_seqno;
		}
		gvcst_jrt_null();
		break;
	default:
		assert(FALSE);
		mur_report_error(jctl, MUR_JNLBADRECFMT);
		return ERR_JNLBADRECFMT;
	}
	return SS_NORMAL;
}
