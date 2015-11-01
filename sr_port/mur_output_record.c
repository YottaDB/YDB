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
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "hashdef.h"
#include "jnl.h"
#include "muprec.h"
#include "copy.h"
#include "util.h"
#include "op.h"
#include "gvcst_root_search.h"
#include "tp_set_sgm.h"
#include "cache.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_begin_crit.h"
#include "t_end.h"
#include "dbfilop.h"
#include "targ_alloc.h"
#include "gvcst_blk_build.h"
#include "hashtab.h"

GBLREF	fixed_jrec_tp_kill_set 	mur_jrec_fixed_field;
GBLREF	struct_jrec_tcom 	mur_jrec_fixed_tcom;
GBLREF	bool			gv_curr_subsc_null;
GBLREF	cw_set_element		cw_set[];
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF  sgmnt_data_ptr_t 	cs_data;
GBLREF	short           	dollar_trestart;
GBLREF	short			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF  mur_opt_struct  	mur_options;
GBLREF	boolean_t		write_after_image;
GBLREF	jnl_fence_control	jnl_fence_ctl; /* Needed to set the token, optimize jnl_write_logical for recover */
GBLREF	boolean_t		losttrans, brktrans;
GBLREF 	uint4			cur_logirec_short_time;	/* see comment in gbldefs.c for usage */

/* The record handling is totally changed due to recover writing journal records, all constant fields are now copied to global
structure, which is subsequently used in mur_jnl_write_logical ( equivalent of jnl_write_logical) */

void	mur_output_record(ctl_list *ctl)
{
	mval			v;
	int			cycle, temp_int;
	int4			blk_size;
	mstr_len_t		*data_len;
	jnl_record		*rec;
	sm_uc_ptr_t		blk_ptr;
	mname			lcl_name;
	char			stashed;
	unsigned char		*c, *c_top, *in, *in_top;
	ht_entry		*h, *ht_put ();
	boolean_t		set_or_kill_record = FALSE;
	uint4			dbfilop();
	uint4			pini_addr, new_pini_addr, dummy;
	fixed_jrec_tp_kill_set	*jrec;

	error_def(ERR_MURAIMGFAIL);
	rec = (jnl_record *)ctl->rab->recbuff;
	assert(!brktrans && !losttrans); /* We shouldn't be here if brktrans or losttrans */

	switch (REF_CHAR(&rec->jrec_type))
	{
	case JRT_TCOM:
		if (ctl->before_image)
		{
			mur_jrec_fixed_tcom.tc_short_time = rec->val.jrec_tcom.tc_short_time;
			mur_jrec_fixed_tcom.rec_seqno = rec->val.jrec_tcom.rec_seqno;
			mur_jrec_fixed_tcom.jnl_seqno = rec->val.jrec_tcom.jnl_seqno;
			jnl_fence_ctl.token = rec->val.jrec_tcom.token;
			mur_jrec_fixed_tcom.participants = rec->val.jrec_tcom.participants;
			mur_jrec_fixed_tcom.ts_short_time = rec->val.jrec_tcom.ts_short_time;
		}
		op_tcommit();
		return;

	case JRT_TSET:
	case JRT_TKILL:
	case JRT_TZKILL:
		v.mvtype = MV_STR;
		/* Copy the tid field from the original tset record only if before image flag is set */
		if (ctl->before_image)
		{
			v.str.len = strlen(rec->val.jrec_tset.jnl_tid);
			if (0 != v.str.len)
				v.str.addr = (char *)rec->val.jrec_tset.jnl_tid;
			else
				v.str.addr = NULL;
		} else
		{
			v.str.len = 0;
			v.str.addr = NULL;
		}
		op_tstart(TRUE, TRUE, &v, -1);
		tp_set_sgm();
		break;
	}
	switch (REF_CHAR(&rec->jrec_type))
	{
	case JRT_SET:
	case JRT_KILL:
	case JRT_ZKILL:
		assert(&rec->val.jrec_set.mumps_node == &rec->val.jrec_kill.mumps_node);
		assert(&rec->val.jrec_set.mumps_node == &rec->val.jrec_zkill.mumps_node);
		memcpy(gv_currkey->base, rec->val.jrec_set.mumps_node.text, rec->val.jrec_set.mumps_node.length);
		gv_currkey->base[rec->val.jrec_set.mumps_node.length] = '\0';
		gv_currkey->end = rec->val.jrec_set.mumps_node.length;
		if (ctl->before_image)
		{
			jrec = (fixed_jrec_tp_kill_set *)&rec->val.jrec_set;
			COPY_MUR_JREC_FIXED_FIELDS(mur_jrec_fixed_field, jrec);
		}
		set_or_kill_record = TRUE;
		break;

	case JRT_INCTN:
		assert(ctl->gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		if (!mur_options.forward)
			return;
		switch(rec->val.jrec_inctn.opcode)
		{
			case inctn_bmp_mark_free_gtm: /* KILL record will take care of this */
			case inctn_gdsfilext_gtm: /* forward recovery will automatically extend for corresponding SET record */
				return;
			case inctn_gdsfilext_mu_reorg:
			case inctn_bmp_mark_free_mu_reorg:
			case inctn_mu_reorg:
			case inctn_gvcstput_extra_blk_split:
			case inctn_wcs_recover:
				cs_data->trans_hist.early_tn = ++cs_data->trans_hist.curr_tn;
				return;
			default:
				GTMASSERT;
		}
                return;

	case JRT_TSET:
	case JRT_USET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_gset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_tset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_uset.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_fkill.mumps_node);
		assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_gkill.mumps_node);
		assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_tkill.mumps_node);
		assert(&rec->val.jrec_fkill.mumps_node == &rec->val.jrec_ukill.mumps_node);
		assert(&rec->val.jrec_fset.mumps_node == &rec->val.jrec_fzkill.mumps_node);
		assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_gzkill.mumps_node);
		assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_tzkill.mumps_node);
		assert(&rec->val.jrec_fzkill.mumps_node == &rec->val.jrec_uzkill.mumps_node);

		memcpy(gv_currkey->base, rec->val.jrec_fset.mumps_node.text, rec->val.jrec_fset.mumps_node.length);
		gv_currkey->base[rec->val.jrec_fset.mumps_node.length] = '\0';
		gv_currkey->end = rec->val.jrec_fset.mumps_node.length;
		if (ctl->before_image)
		{
			jrec = (fixed_jrec_tp_kill_set *)&rec->val.jrec_fset;
			COPY_MUR_JREC_FIXED_FIELDS(mur_jrec_fixed_field, jrec);
			QWASSIGN(jnl_fence_ctl.token, rec->val.jrec_fset.token);
		}
		set_or_kill_record = TRUE;
		break;
	}

	if (set_or_kill_record)
	{
		if (ctl->before_image)
		{
			pini_addr = rec->val.jrec_kill.pini_addr;
			new_pini_addr = (uint4)lookup_hashtab_ent(ctl->pini_in_use, (void *)pini_addr, &dummy);
			/* new_pini_addr might not be in the hashtable if the corresponding PINI record lies before
			 * the turnaround point. In that case we can use the pini_addr from the journal record directly.
			 */
			cs_addrs->jnl->pini_addr = new_pini_addr ? new_pini_addr : rec->val.jrec_kill.pini_addr;
		}
		for (c = (unsigned char *)&lcl_name, c_top = c + sizeof(lcl_name),
				in = (unsigned char *)gv_currkey->base, in_top = in + sizeof(mname); in < in_top && *in; )
			*c++ = *in++;
		assert(!*in);
		while (c < c_top)
			*c++ = 0;
		h = ht_put((htab_desc *)ctl->tab_ptr, &lcl_name, &stashed);
		if (!stashed && h->ptr)
		{
			gv_target = (gv_namehead*)h->ptr;
			assert(gv_target->gd_reg->open);
			if (dollar_trestart)
				gv_target->clue.end = 0;
		} else
		{
			assert(gv_cur_region->max_key_size <= MAX_KEY_SZ);
			gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size);
			gv_target->gd_reg = gv_cur_region;
			h->ptr = (char *)gv_target;
			memcpy(&gv_target->gvname, &lcl_name, sizeof(mident));
		}
		if (gv_target->root == 0 || gv_target->root == DIR_ROOT)
		{
			assert(gv_target != cs_addrs->dir_tree);
			gvcst_root_search();
		}
	}

	switch (REF_CHAR(&rec->jrec_type))
	{
	case JRT_PBLK:
		/* This only takes place during rollback, and is thus the first restoration
		   being done to the database;  therefore, it will not cause a conflict with
		   the write cache, as the cache will be empty */

		ctl->db_ctl->op = FC_WRITE;
		ctl->db_ctl->op_buff = (uchar_ptr_t)rec->val.jrec_pblk.blk_contents;
		ctl->db_ctl->op_len = (rec->val.jrec_pblk.bsiz + 7) & ~7;
		ctl->db_ctl->op_pos = FILE_INFO(ctl->gd)->s_addrs.hdr->blk_size / DISK_BLOCK_SIZE * rec->val.jrec_pblk.blknum
				      + FILE_INFO(ctl->gd)->s_addrs.hdr->start_vbn;
		dbfilop(ctl->db_ctl);
		return;

	case JRT_AIMG:
		if (ctl->before_image)
		{
			pini_addr = rec->val.jrec_kill.pini_addr;
			new_pini_addr = (uint4)lookup_hashtab_ent(ctl->pini_in_use, (void *)pini_addr, &dummy);
			/* new_pini_addr might not be in the hashtable if the corresponding PINI record lies before
			 * the turnaround point. In that case we can use the pini_addr from the journal record directly.
			 */
			cs_addrs->jnl->pini_addr = new_pini_addr ? new_pini_addr : rec->val.jrec_kill.pini_addr;
		}
		assert(ctl->gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		assert(!dollar_tlevel);
		if (!mur_options.apply_after_image)
			return;
		write_after_image = TRUE;
		mur_put_aimg_rec(rec);
		write_after_image = FALSE;
		return;

	case JRT_SET:
		data_len = (mstr_len_t *)((char *)&rec->val.jrec_set.mumps_node
						+ rec->val.jrec_set.mumps_node.length + sizeof(short));
		v.mvtype = MV_STR;
		GET_MSTR_LEN(v.str.len, data_len);
		v.str.addr = (char *)data_len + sizeof(mstr_len_t);
		op_gvput(&v);
		return;

	case JRT_TSET:
	case JRT_USET:
	case JRT_FSET:
	case JRT_GSET:
		data_len = (mstr_len_t *)((char *)&rec->val.jrec_fset.mumps_node
						+ rec->val.jrec_fset.mumps_node.length + sizeof(short));
		v.mvtype = MV_STR;
		GET_MSTR_LEN(v.str.len, data_len);
		v.str.addr = (char *)data_len + sizeof(mstr_len_t);
		op_gvput(&v);
		return;

	case JRT_TKILL:
	case JRT_UKILL:
		tp_set_sgm(); 	/* drop through ... */

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_KILL:
		op_gvkill();
		return;

	case JRT_TZKILL:
	case JRT_UZKILL:
		tp_set_sgm(); 	/* drop through ... */

	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_ZKILL:
		op_gvzwithdraw();
		return;

	default:
		mur_report_error(ctl, MUR_UNKNOWN);
	}

}
