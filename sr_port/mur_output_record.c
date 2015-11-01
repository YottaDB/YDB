/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "copy.h"
#include "util.h"
#include "op.h"
#include "tp_set_sgm.h"
#include "cache.h"
#include "gtmmsg.h"
#include "jnl_typedef.h"
#include "iosp.h"		/* for SS_NORMAL */

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_begin_crit.h"
#include "t_end.h"
#include "dbfilop.h"
#include "targ_alloc.h"
#include "gvcst_blk_build.h"
#include "hashtab.h"
#include "jnl_write.h"

GBLREF  mur_opt_struct  	mur_options;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF 	mur_rab_t		mur_rab;
GBLREF	jnl_ctl_list		*mur_jctl;
GBLREF 	int			mur_regno;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF  sgmnt_data_ptr_t 	cs_data;
GBLREF	short			dollar_tlevel;
GBLREF	boolean_t		write_after_image;
GBLREF	jnl_fence_control	jnl_fence_ctl; /* Needed to set the token, optimize jnl_write_logical for recover */
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF  jnl_process_vector	*prc_vec;
GBLREF 	mur_gbls_t		murgbl;

LITREF	int			jrt_update[];
LITREF	boolean_t		jrt_is_replicated[JRT_RECTYPES];

/* This routine is called only for recover and rollback (that is, mur_options.update).
 * It applies the set/kill/zkill, tcom, inctn, and aimg records during forward processing.
 * Some fields like jnl_seqno, rec_seqno and prefix.time are saved here from original journal files.
 * Later jnl_write routines copies them to journal records instead of generating them like run_time system */
uint4	mur_output_record()
{
	mval			mv;
	jnl_record		*rec;
	char			*val_ptr;
	uint4			dummy;
	off_jnl_t		pini_addr;
	jnl_string		*keystr;
	enum jnl_record_type 	rectype;
	uint4			jnl_status, status;
	pini_list_struct	*plst;
	boolean_t		jnl_enabled, was_crit;
	struct_jrec_null	null_record;

	error_def(ERR_JNLBADRECFMT);

	assert(mur_options.update);
	rec = mur_rab.jnlrec;
	rectype = rec->prefix.jrec_type;
	if (JRT_ALIGN == rectype || JRT_EOF == rectype || JRT_EPOCH == rectype || JRT_PBLK == rectype || JRT_PINI == rectype)
		return SS_NORMAL;
	jgbl.gbl_jrec_time = rec->prefix.time;
	pini_addr = rec->prefix.pini_addr;
	assert(cs_addrs == mur_ctl[mur_regno].csa);
	jnl_enabled = JNL_ENABLED(cs_addrs);
	if (jnl_enabled)
	{
		status = mur_get_pini(pini_addr, &plst);
		if (SS_NORMAL != status)
			return status;
		prc_vec = &plst->jpv;
		cs_addrs->jnl->pini_addr = plst->new_pini_addr;
		jgbl.mur_plst = plst;
		if (mur_options.rollback && IS_REPLICATED(rectype))
		{
			jgbl.mur_jrec_seqno = ((struct_jrec_ztp_upd *)mur_rab.jnlrec)->jnl_seqno;
			if (jgbl.mur_jrec_seqno >= murgbl.consist_jnl_seqno)
				murgbl.consist_jnl_seqno = jgbl.mur_jrec_seqno + 1;
		}
	}
	if (IS_SET_KILL_ZKILL(rectype))
	{
		if (IS_ZTP(rectype))
		{	/* ZTP has different record format than TP or non-TP */
			keystr = (jnl_string *)&rec->jrec_fkill.mumps_node;
			if (jnl_enabled)
				jgbl.mur_jrec_token_seq.token = rec->jrec_fset.token;
		} else
		{	/* TP and non-TP has same format */
			keystr = (jnl_string *)&rec->jrec_kill.mumps_node;
			if (jnl_enabled)
				jgbl.mur_jrec_token_seq = rec->jrec_set.token_seq;
		}
		if (IS_FENCED(rectype))
		{	/* Even for FENCE_NONE we apply fences. Otherwise an [F/G/T/U]UPD becomes UPD etc. */
			/* op_tstart is called in mur_forward already */
			if (IS_FUPD(rectype))
			{
				jnl_fence_ctl.level = 1;
				if (jnl_enabled)
				{
					jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
					cs_addrs->next_fenced = NULL;
				}
			} else if (IS_GUPD(rectype))
			{
				jnl_fence_ctl.level = 1;
				if (jnl_enabled)
				{
					jnl_fence_ctl.fence_list = cs_addrs;
					cs_addrs->next_fenced = (sgmnt_addrs *)-1;
				}
			} else if (IS_TP(rectype))
				tp_set_sgm();
		}
		if (IS_SET(rectype))
		{
			val_ptr = &keystr->text[keystr->length];
			GET_MSTR_LEN(mv.str.len, val_ptr);
			mv.str.addr = val_ptr + sizeof(mstr_len_t);
			mv.mvtype = MV_STR;
			op_gvput(&mv);
		} else if (IS_KILL(rectype))
		{
			if (IS_TP(rectype))
				tp_set_sgm();
			op_gvkill();
		} else
		{
			if (IS_TP(rectype))
				tp_set_sgm();
			op_gvzwithdraw();
		}
		if (IS_ZTP(rectype))
		{	/* Even for FENCE_NONE we apply fences. Otherwise an FUPD/GUPD becomes UPD etc. */
			assert(jnl_enabled ||
				((sgmnt_addrs *)-1 == jnl_fence_ctl.fence_list && NULL == cs_addrs->next_fenced));
			jnl_fence_ctl.level = 0;
			if (jnl_enabled)
			{
				jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
				cs_addrs->next_fenced = NULL;
			}
		}
		return SS_NORMAL;
	}
	switch(rectype)
	{
	case JRT_TCOM:
		/* Even for FENCE_NONE we apply fences. Otherwise an TUPD/UUPD becomes UPD etc. */
		if (jnl_enabled)
		{
			jgbl.mur_jrec_token_seq = rec->jrec_tcom.token_seq;
			jgbl.mur_jrec_participants = rec->jrec_tcom.participants;
			memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE);
		}
		op_tcommit();
		return SS_NORMAL;
	case JRT_ZTCOM:
		/* Even for FENCE_NONE we apply fences. Otherwise an FUPD/GUPD becomes UPD etc. */
		if (jnl_enabled)
		{
			jgbl.mur_jrec_token_seq.token = rec->jrec_ztcom.token;
			jgbl.mur_jrec_participants = rec->jrec_ztcom.participants;
		}
		jnl_fence_ctl.level = 1;
		if (jnl_enabled)
		{
			jnl_fence_ctl.fence_list = cs_addrs;
			cs_addrs->next_fenced = (sgmnt_addrs *)-1;
		}
		op_ztcommit(1);
		assert(jnl_enabled ||
			((sgmnt_addrs *)-1 == jnl_fence_ctl.fence_list && NULL == cs_addrs->next_fenced));
		jnl_fence_ctl.level = 0;
		jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
		cs_addrs->next_fenced = NULL;
		return SS_NORMAL;
	case JRT_INCTN:
		assert(mur_ctl[mur_regno].gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(mur_ctl[mur_regno].gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		if (mur_options.forward)
		{
			assert(rec->jrec_inctn.prefix.tn == cs_data->trans_hist.curr_tn || mur_options.notncheck);
			cs_data->trans_hist.early_tn = ++cs_data->trans_hist.curr_tn;
		}
                return SS_NORMAL;
	case JRT_AIMG:
		assert(mur_ctl[mur_regno].gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(mur_ctl[mur_regno].gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		assert(!dollar_tlevel);
		if (!mur_options.apply_after_image)
			return SS_NORMAL;
		write_after_image = TRUE;
		mur_put_aimg_rec(rec);
		write_after_image = FALSE;
		return SS_NORMAL;
	case JRT_PFIN:
		if (jnl_enabled)
		{
			if (FALSE == ((was_crit = mur_ctl[mur_regno].csa->now_crit)))
				grab_crit(mur_ctl[mur_regno].gd);
			assert(gv_cur_region == mur_ctl[mur_regno].gd);
			jnl_status = jnl_ensure_open();
			if (0 != jnl_status)
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			assert(plst->new_pini_addr == cs_addrs->jnl->pini_addr);
			if (0 != cs_addrs->jnl->pini_addr)
				jnl_put_jrt_pfin(mur_ctl[mur_regno].csa);
			rel_crit(mur_ctl[mur_regno].gd);
		}
		return SS_NORMAL;
	case JRT_NULL:
		if (jnl_enabled)
		{
			grab_crit(mur_ctl[mur_regno].gd);
			assert(gv_cur_region == mur_ctl[mur_regno].gd);
			cs_addrs->ti->early_tn = cs_addrs->ti->curr_tn + 1;
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				if (0 == cs_addrs->jnl->pini_addr)
					jnl_put_jrt_pini(cs_addrs);
				null_record.prefix.jrec_type = JRT_NULL;
				null_record.prefix.forwptr = null_record.suffix.backptr = NULL_RECLEN;
				null_record.prefix.time = jgbl.gbl_jrec_time;
				null_record.prefix.tn = cs_addrs->ti->curr_tn;
				null_record.prefix.pini_addr = cs_addrs->jnl->pini_addr;
				null_record.jnl_seqno = jgbl.mur_jrec_seqno;
				null_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
				jnl_write(cs_addrs->jnl, JRT_NULL, (jnl_record *)&null_record, NULL, NULL);
			} else
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			cs_addrs->ti->curr_tn = cs_addrs->ti->early_tn;
			rel_crit(gv_cur_region);
		}
		return SS_NORMAL;
	default:
		assert(FALSE);
		mur_report_error(MUR_JNLBADRECFMT);
		return ERR_JNLBADRECFMT;
	}
	return SS_NORMAL;
}
