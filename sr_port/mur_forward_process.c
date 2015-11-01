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

#include "gtm_string.h"

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
#include "hashtab.h"

GBLREF	boolean_t		brktrans;
GBLREF	boolean_t		losttrans;
GBLREF	mur_opt_struct		mur_options;
GBLREF	seq_num			stop_rlbk_seqno;
GBLREF  seq_num			resync_jnl_seqno;
GBLREF	seq_num			consist_jnl_seqno;
GBLREF	seq_num			seq_num_one;
GBLREF  gv_key          	*gv_currkey;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t 	cs_data;
GBLREF  gd_region		*gv_cur_region;
GBLREF  jnl_process_vector	*prc_vec;
GBLREF	jnl_process_vector	*originator_prc_vec;
GBLREF	jnl_process_vector	*server_prc_vec;
GBLREF	fixed_jrec_tp_kill_set 	mur_jrec_fixed_field;
LITREF	int			jnl_fixed_size[];

static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, size)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

/* The handling of PINI and PFIN record by recover is modified. We keep track of the pini_addr and PINI records using hashtable.
We store the pini_addr for the PINI record we encountered in a hashtable and subsequent Journal records (SET/KILL etc) use
this hashtable to get the pini_addr. This is necessiated due to the reordering of journal records written to journal file. */

bool	mur_forward_process(ctl_list *ctl)
{
	boolean_t		ret_val;
	seq_num			rb_jnl_seqno;
	jnl_process_vector	*pv;
	jnl_record		*rec;
	enum jnl_record_type	rectype;
	uint4			pini_addr, status, jnl_status = 0, new_pini_addr, dummy;
	uint4			rec_time;
	token_num		token;
	fi_type			*fi;
	char			ext_buff[100];
	void			(*extract)();
	struct_jrec_pfin	pfin_record;
	jnl_process_vector	*save_prc_vec;


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
		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_pfin.process_vector);
		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_eof.process_vector);
		if (ctl->before_image && mur_options.update && !losttrans && !brktrans)
		{	/* This is to calculate the original pini_addr in the original journal file, with Idempotency
			 * recovery the journal file is truncated and a copy of the data exists in new journal file.
			 * This is the case where we find a PINI record in a temporary copy of new journal file. The
			 * new journal file contains all the data from consist_stop_addr in old journal file at the
			 * rab->dskaddr onwards, hence the calculation.
			 */
			grab_crit(ctl->gd);
			assert(gv_cur_region == ctl->gd);
			jnl_status = jnl_ensure_open();
			if (jnl_status == 0)
			{	/* calculate pini_addr for two cases.
				 *	(i) if this ctl doesn't have a .forw_phase file then take ctl->rab->dskaddr as it is.
				 *	(ii) if this ctl has a .forw_phase file then need to do some repositioning
				 *		since the offset of the journal records that were ftruncated and moved into
				 *		the .forw_phase file are now different. ctl->consist_stop_addr points to
				 *		the turnaround point for the ctl and ctl->stop_addr points to the offset
				 *		of the first valid record in the .forw_phase file corresponding to ctl.
				 * ctl->consist_stop_addr is 0 in case there is no .forw_phase file created for this ctl.
				 */
				pini_addr = ctl->rab->dskaddr;
				if (ctl->consist_stop_addr)
					pini_addr += (ctl->consist_stop_addr - ctl->stop_addr);
				jnl_prc_vector(prc_vec);
				assert(NULL == originator_prc_vec);
				assert(NULL == server_prc_vec);
				originator_prc_vec = &rec->val.jrec_pini.process_vector[ORIG_JPV];
				server_prc_vec = &rec->val.jrec_pini.process_vector[SRVR_JPV];
				/* jnl_put_jrt_pini sets  cs_addrs->jnl->regnum, but it's ok as it's not used any
				 * more by recover/rollback */
				jnl_put_jrt_pini(cs_addrs);
				originator_prc_vec = NULL;
				server_prc_vec = NULL;
				new_pini_addr = cs_addrs->jnl->pini_addr;
				add_hashtab_ent(&ctl->pini_in_use, (void *)pini_addr, (void *)new_pini_addr);
			} else
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			rel_crit(ctl->gd);
		} else
			pini_addr = 0;
		rec_time = JNL_S_TIME_PINI(rec, jrec_pini, CURR_JPV);
		ret_val = TRUE;
		break;
	case JRT_PFIN:
		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_pfin.process_vector);
		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_eof.process_vector);
		if (ctl->before_image && mur_options.update && !losttrans && !brktrans)
		{
			grab_crit(ctl->gd);
			assert(gv_cur_region == ctl->gd);
			jnl_status = jnl_ensure_open();
			if (jnl_status == 0)
			{
				pini_addr = rec->val.jrec_pfin.pini_addr;
				new_pini_addr = (uint4)lookup_hashtab_ent(ctl->pini_in_use, (void *)pini_addr, &dummy);
				cs_addrs->jnl->pini_addr = (new_pini_addr ? new_pini_addr : rec->val.jrec_pfin.pini_addr);
				/* prc_vec is initialized in gvcst_init() time of recovery and is necessary during gv_rundown()
				 * time of recovery. In between, we want to use the pfin-jnl-record's prc_vec for simulating
				 * a PFIN update, hence the temporary copy and restore of prc_vec.
				 */
				save_prc_vec = prc_vec;
				prc_vec = &rec->val.jrec_pfin.process_vector;
				jnl_put_jrt_pfin(cs_addrs);
				prc_vec = save_prc_vec;
			} else
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
			rel_crit(ctl->gd);
		} else
			pini_addr = 0;
		rec_time = JNL_S_TIME_PINI(rec, jrec_pini, CURR_JPV);
		ret_val = TRUE;
		break;
	case JRT_EOF:

		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_pfin.process_vector);
		assert(&rec->val.jrec_pini.process_vector[CURR_JPV] == &rec->val.jrec_eof.process_vector);
		pini_addr = 0;
		rec_time = JNL_S_TIME_PINI(rec, jrec_pini, CURR_JPV);
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
					!mur_lookup_multi(ctl, pini_addr, token, rec->val.jrec_fset.jnl_seqno )))
				{
					if (FALSE == losttrans && FALSE == brktrans)
					{
						if (mur_options.rollback)
						{
							QWASSIGN(rb_jnl_seqno, rec->val.jrec_kill.jnl_seqno);
							if (QWGE(rb_jnl_seqno, stop_rlbk_seqno) ||
								(mur_options.fetchresync  &&  QWGE(rb_jnl_seqno, resync_jnl_seqno)))
							{
								losttrans = TRUE;
								break;
							}
							else if (QWLE(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno))
								QWADD(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno, seq_num_one);
						}
							mur_output_record(ctl);
					}
				} else
					brktrans = TRUE;
			}
			break;
		case JRT_SET:
		case JRT_KILL:
		case JRT_ZKILL:
			if (ret_val  &&  mur_options.update  &&  mur_options.fences != FENCE_ALWAYS)
			{
				if (FALSE == losttrans && FALSE == brktrans)
				{
					if (mur_options.rollback)
					{
						QWASSIGN(rb_jnl_seqno, rec->val.jrec_kill.jnl_seqno);
						if (QWGE(rb_jnl_seqno, stop_rlbk_seqno) ||
							(mur_options.fetchresync  &&  QWGE(rb_jnl_seqno, resync_jnl_seqno)))
						{
							losttrans = TRUE;
							break;
						}
						else if (QWLE(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno))
							QWADD(consist_jnl_seqno, rec->val.jrec_kill.jnl_seqno, seq_num_one);
					}
						mur_output_record(ctl);
				}
			}
			break;
		case JRT_INCTN:
		case JRT_AIMG:
			if (losttrans || brktrans) /* INCTN & AIMG should not be in losttrans file as they aren't replicated */
				return TRUE;
			else if (ret_val  &&  mur_options.update  &&  mur_options.fences != FENCE_ALWAYS)
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
