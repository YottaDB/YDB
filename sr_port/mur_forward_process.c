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

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "copy.h"
#include "mur_ext_set.h"

GBLREF	boolean_t	brktrans;
GBLREF	mur_opt_struct	mur_options;
GBLREF	seq_num		stop_rlbk_seqno;
GBLREF  seq_num		resync_jnl_seqno;
GBLREF	seq_num		consist_jnl_seqno;
GBLREF	seq_num		seq_num_one;
GBLREF  gv_key          *gv_currkey;



/* Extraction routines - NOTE:  make sure that all routines listed in JNL_REC_TABLE.H are declared here */
static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, size)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};


bool	mur_forward_process(ctl_list *ctl)
{
	boolean_t		ret_val;
	boolean_t		losttrans = FALSE;
	seq_num			rb_jnl_seqno;
	jnl_process_vector	*pv;
	jnl_record		*rec;
	enum jnl_record_type	rectype;
	uint4			pini_addr, status;
	uint4			rec_time;
	token_num		token;
	fi_type			*fi;
	char			ext_buff[100];
	void			(*extract)();


	error_def(ERR_JNLEXTR);

	rec = (jnl_record *)ctl->rab->recbuff;
	rectype = REF_CHAR(&rec->jrec_type);

	switch (rectype)
	{
	default:
		return mur_report_error(ctl, MUR_UNKNOWN);

	case JRT_NULL:
		pini_addr = 0;
		rec_time = rec->val.jrec_null.short_time;
		break;

	case JRT_PBLK:
	case JRT_EPOCH:
	case JRT_ALIGN:

		/* These should never be seen here */
		assert(FALSE);
		return TRUE;

	case JRT_PINI:
	case JRT_PFIN:
	case JRT_EOF:

		assert(&rec->val.jrec_pini.process_vector == &rec->val.jrec_pfin.process_vector);
		assert(&rec->val.jrec_pini.process_vector == &rec->val.jrec_eof.process_vector);

		pini_addr = 0;
		rec_time = JNL_S_TIME(rec, jrec_pini);
		ret_val = TRUE;

		break;


	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_KILL:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_TCOM:
	case JRT_ZTCOM:
	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
	case JRT_INCTN:
	case JRT_AIMG:

		pini_addr = rec->val.jrec_set.pini_addr;

		assert(pini_addr == rec->val.jrec_fset.pini_addr);
		assert(pini_addr == rec->val.jrec_gset.pini_addr);
		assert(pini_addr == rec->val.jrec_tset.pini_addr);
		assert(pini_addr == rec->val.jrec_uset.pini_addr);
		assert(pini_addr == rec->val.jrec_kill.pini_addr);
		assert(pini_addr == rec->val.jrec_fkill.pini_addr);
		assert(pini_addr == rec->val.jrec_gkill.pini_addr);
		assert(pini_addr == rec->val.jrec_tkill.pini_addr);
		assert(pini_addr == rec->val.jrec_ukill.pini_addr);
		assert(pini_addr == rec->val.jrec_tcom.pini_addr);
		assert(pini_addr == rec->val.jrec_ztcom.pini_addr);
		assert(pini_addr == rec->val.jrec_zkill.pini_addr);
		assert(pini_addr == rec->val.jrec_fzkill.pini_addr);
		assert(pini_addr == rec->val.jrec_gzkill.pini_addr);
		assert(pini_addr == rec->val.jrec_tzkill.pini_addr);
		assert(pini_addr == rec->val.jrec_uzkill.pini_addr);
		assert(pini_addr == rec->val.jrec_inctn.pini_addr);
		assert(pini_addr == rec->val.jrec_aimg.pini_addr);

		rec_time = rec->val.jrec_set.short_time;

		assert(rec_time == rec->val.jrec_fset.short_time);
		assert(rec_time == rec->val.jrec_gset.short_time);
		assert(rec_time == rec->val.jrec_tset.short_time);
		assert(rec_time == rec->val.jrec_uset.short_time);
		assert(rec_time == rec->val.jrec_kill.short_time);
		assert(rec_time == rec->val.jrec_fkill.short_time);
		assert(rec_time == rec->val.jrec_gkill.short_time);
		assert(rec_time == rec->val.jrec_tkill.short_time);
		assert(rec_time == rec->val.jrec_ukill.short_time);
		assert(rec_time == rec->val.jrec_tcom.tc_short_time);
		assert(rec_time == rec->val.jrec_ztcom.tc_short_time);
		assert(rec_time == rec->val.jrec_zkill.short_time);
		assert(rec_time == rec->val.jrec_fzkill.short_time);
		assert(rec_time == rec->val.jrec_gzkill.short_time);
		assert(rec_time == rec->val.jrec_tzkill.short_time);
		assert(rec_time == rec->val.jrec_uzkill.short_time);
		assert(rec_time == rec->val.jrec_inctn.short_time);
		assert(rec_time == rec->val.jrec_aimg.short_time);

		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_gset.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tset.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_uset.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_fkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_gkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_ukill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tcom.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_ztcom.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_fzkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_gzkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tzkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_uzkill.token);

		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_fset.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_gset.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_tset.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_uset.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_kill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_fkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_gkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_tkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_ukill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_tcom.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_ztcom.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_zkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_fzkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_gzkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_tzkill.jnl_seqno);
		assert(&rec->val.jrec_set.jnl_seqno ==  &rec->val.jrec_uzkill.jnl_seqno);

		ret_val = !mur_options.before  ||  rec_time <= JNL_M_TIME(before_time);

		switch (rectype)
		{
		case JRT_FSET:
		case JRT_GSET:
		case JRT_TSET:
		case JRT_USET:
		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_TCOM:		/* But NOT ZTCOM */
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:
		case JRT_UZKILL:
			if (!mur_options.notncheck  &&  mur_options.forward  &&
					((trans_num)-1 != ctl->jnl_tn)  &&  ctl->jnl_tn != ctl->db_tn)
				ret_val = mur_report_error(ctl, MUR_TNCHECK);

			if (ret_val  &&  mur_options.update)
			{
				QWASSIGN(token, rec->val.jrec_fset.token);

				assert(QWEQ(token, rec->val.jrec_gset.token));
				assert(QWEQ(token, rec->val.jrec_tset.token));
				assert(QWEQ(token, rec->val.jrec_uset.token));
				assert(QWEQ(token, rec->val.jrec_fkill.token));
				assert(QWEQ(token, rec->val.jrec_gkill.token));
				assert(QWEQ(token, rec->val.jrec_tkill.token));
				assert(QWEQ(token, rec->val.jrec_ukill.token));
				assert(QWEQ(token, rec->val.jrec_tcom.token));
				assert(QWEQ(token, rec->val.jrec_fzkill.token));
				assert(QWEQ(token, rec->val.jrec_gzkill.token));
				assert(QWEQ(token, rec->val.jrec_tzkill.token));
				assert(QWEQ(token, rec->val.jrec_uzkill.token));

				if ((mur_options.fences == FENCE_NONE) || (!mur_lookup_broken(ctl, pini_addr, token)  &&
					!mur_lookup_multi(ctl, pini_addr, token )))
				{
					losttrans = FALSE;
					if (mur_options.rollback)
					{
						QWASSIGN(rb_jnl_seqno, rec->val.jrec_kill.jnl_seqno);
						if (QWGE(rb_jnl_seqno, stop_rlbk_seqno) ||
								(mur_options.fetchresync  &&  QWGE(rb_jnl_seqno, resync_jnl_seqno)))
							losttrans = TRUE;
						else
						{
							ctl->consist_stop_addr = ctl->rab->dskaddr;
							if (QWLE(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno))
								QWADD(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno, seq_num_one);
						}
					}
					else
						ctl->consist_stop_addr = ctl->rab->dskaddr;
					if (FALSE == losttrans && FALSE == brktrans)
						mur_output_record(ctl);
				}
				else
					brktrans = TRUE;
			}
			break;

		case JRT_SET:
		case JRT_KILL:
		case JRT_ZKILL:
			if (ret_val  &&  mur_options.update  &&  mur_options.fences != FENCE_ALWAYS)
			{
				losttrans = FALSE;
				if (mur_options.rollback)
				{
					QWASSIGN(rb_jnl_seqno , rec->val.jrec_kill.jnl_seqno);
					if (QWGE(rb_jnl_seqno, stop_rlbk_seqno)  ||
							(mur_options.fetchresync  &&  QWGE(rb_jnl_seqno, resync_jnl_seqno)))
						losttrans = TRUE;
					else
					{
						ctl->consist_stop_addr = ctl->rab->dskaddr;
						if (QWLE(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno))
							QWADD(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno, seq_num_one);
					}
				}
				else
					ctl->consist_stop_addr = ctl->rab->dskaddr;
				if (FALSE == losttrans && FALSE == brktrans)
					mur_output_record(ctl);
			}
			break;
		case JRT_INCTN:
			if (ret_val  &&  mur_options.update  &&  mur_options.fences != FENCE_ALWAYS)
			{
				if (!mur_options.rollback)
					ctl->consist_stop_addr = ctl->rab->dskaddr;
				if (FALSE == losttrans && FALSE == brktrans)
					mur_output_record(ctl);
			}
			break;
		case JRT_AIMG:
			if (ret_val  &&  mur_options.update  &&  mur_options.fences != FENCE_ALWAYS)
				if (FALSE == losttrans && FALSE == brktrans)
					mur_output_record(ctl);
			break;
		}
	}

	if (ret_val  &&  mur_options.extr_file_info != NULL  &&  !mur_options.detail  &&
	    (!mur_options.forward  ||
	     (!mur_options.before  ||  rec_time <= JNL_M_TIME(before_time))  &&
	     (!mur_options.since  ||  rec_time >= JNL_M_TIME(since_time))))
	{
		extract = extraction_routine[rectype];
		assert(extract != NULL);

		if (pini_addr == 0)
			(*extract)(rec);
		else
			if ((pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
				(*extract)(rec, pv->jpv_pid, &pv->jpv_time);
	}
	if (losttrans || brktrans)
	{
		switch (rectype)
		{
		case JRT_SET:
			memcpy(gv_currkey->base, rec->val.jrec_set.mumps_node.text, rec->val.jrec_set.mumps_node.length);
			gv_currkey->base[rec->val.jrec_set.mumps_node.length] = '\0';
			gv_currkey->end = rec->val.jrec_set.mumps_node.length;
			break;

		case JRT_KILL:
		case JRT_ZKILL:
			assert(&rec->val.jrec_kill.mumps_node == &rec->val.jrec_zkill.mumps_node);
			memcpy(gv_currkey->base, rec->val.jrec_kill.mumps_node.text, rec->val.jrec_kill.mumps_node.length);
			gv_currkey->base[rec->val.jrec_kill.mumps_node.length] = '\0';
			gv_currkey->end = rec->val.jrec_kill.mumps_node.length;
			break;

		case JRT_FSET:
		case JRT_GSET:
		case JRT_TSET:
		case JRT_USET:
			memcpy(gv_currkey->base, rec->val.jrec_fset.mumps_node.text, rec->val.jrec_fset.mumps_node.length);
			gv_currkey->base[rec->val.jrec_fset.mumps_node.length] = '\0';
			gv_currkey->end = rec->val.jrec_fset.mumps_node.length;
			break;

		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:
		case JRT_UZKILL:
			assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_fzkill.mumps_node);
			memcpy(gv_currkey->base, rec->val.jrec_fkill.mumps_node.text, rec->val.jrec_fkill.mumps_node.length);
			gv_currkey->base[rec->val.jrec_fkill.mumps_node.length] = '\0';
			gv_currkey->end = rec->val.jrec_fkill.mumps_node.length;
			break;
		}
	}
	if (brktrans)
	{
		mur_brktrans_open_files(ctl);
		extract = extraction_routine[rectype];
		assert(extract != NULL);

		if (pini_addr == 0)
			(*extract)(rec);
		else
		   if ((pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
			(*extract)(rec, pv->jpv_pid, &pv->jpv_time);
	}
	if (losttrans)
	{
		extract = extraction_routine[rectype];
		assert(extract != NULL);
		if (mur_options.detail)
			jnlext1_write(ctl);
		if (pini_addr == 0)
			(*extract)(rec);
		else
			if ((pv = mur_get_pini_jpv(ctl, pini_addr)) != NULL)
				(*extract)(rec, pv->jpv_pid, &pv->jpv_time);
	}

	return ret_val;
}
