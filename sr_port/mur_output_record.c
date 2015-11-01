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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
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

GBLREF	bool		gv_curr_subsc_null;
GBLREF	cw_set_element	cw_set[];
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF	short		dollar_trestart;
GBLREF	short		dollar_tlevel;
GBLREF	gd_region	*gv_cur_region;
GBLREF	mur_opt_struct	mur_options;
GBLREF	char		*update_array, *update_array_ptr;
GBLREF	int		update_array_size;
GBLREF	srch_hist	dummy_hist;
GBLREF	unsigned char	*non_tp_jfb_buff_ptr;

void	mur_output_record(ctl_list *ctl)
{
	blk_segment	*bs1, *bs_ptr;
	boolean_t	set_or_kill_record = FALSE;
	char		stashed;
	cw_set_element	*cse;
	ht_entry	*h;
	int4		blk_seg_cnt, blk_size;	/* needed for the BLK_* macros */
	jnl_record	*rec;
	mname		lcl_name;
	mstr_len_t	*data_len;
	mval		v;
	sm_uc_ptr_t	blk_ptr, aimg_blk_ptr;
	unsigned char	*c, *c_top, *in, *in_top, level;
	unsigned short	bsize;

	error_def(ERR_MURAIMGFAIL);

	rec = (jnl_record *)ctl->rab->recbuff;

	switch (REF_CHAR(&rec->jrec_type))
	{
	case JRT_TCOM:
		op_tcommit();
		return;

	case JRT_TSET:
	case JRT_TKILL:
	case JRT_TZKILL:
		v.mvtype = MV_STR;
		v.str.len = 0;
		v.str.addr = NULL;
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
		set_or_kill_record = TRUE;
		break;

	case JRT_INCTN:
		assert(ctl->gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		switch(rec->val.jrec_inctn.opcode)
		{
			case inctn_bmp_mark_free_gtm: /* KILL record will take care of this */
				return;
			case inctn_gdsfilext_gtm: /* forward recovery will automatically extend for corresponding SET record */
				if (mur_options.forward)
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
		set_or_kill_record = TRUE;
		break;
	}
	if (set_or_kill_record)
	{
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
		assert(ctl->gd == gv_cur_region);
		assert(cs_addrs == (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs);
		assert(cs_data == cs_addrs->hdr);
		assert(!dollar_tlevel);
		if (!mur_options.apply_after_image)
			return;
		/* Applying an after image record should use t_begin/t_end mechanisms instead of just copying over
		 * the aimg block into the t_qread buffer. This is because there are lots of other things like
		 * making the cache-record become dirty in case of BG and some others to do in case of MM.
		 * Therefore, it is best to call t_end().
		 */
		assert(update_array);
		/* reset new block mechanism */
		update_array_ptr = update_array;
		assert(!cs_addrs->now_crit);
		blk_size = cs_addrs->hdr->blk_size;
		t_begin_crit(ERR_MURAIMGFAIL);	/* this grabs crit */
		if (NULL == (blk_ptr = t_qread(rec->val.jrec_aimg.blknum, &dummy_hist.h[0].cycle, &dummy_hist.h[0].cr)))
			GTMASSERT;
		aimg_blk_ptr = (sm_uc_ptr_t)&rec->val.jrec_aimg.blk_contents[0];
		/* The two macros GET_USHORT and GET_CHAR are required currently to fix the unaligned acces errors due to
			dereferencing bsiz and will be fixed with a change to journal record struct as part of idem_potency
			recover changes */

		GET_USHORT(bsize, &((blk_hdr_ptr_t)aimg_blk_ptr)->bsiz);
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (uchar_ptr_t)aimg_blk_ptr + sizeof(blk_hdr),
					(int)bsize - sizeof(blk_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
			GTMASSERT;
		GET_CHAR(level, &((blk_hdr_ptr_t)aimg_blk_ptr)->levl);
		t_write(rec->val.jrec_aimg.blknum, (unsigned char *)bs1, 0, 0,
					blk_ptr, level, TRUE, FALSE);
		BUILD_AIMG_IF_JNL_ENABLED(cs_addrs, cs_data, non_tp_jfb_buff_ptr, cse);
		t_end(&dummy_hist, 0);
		assert(!cs_addrs->now_crit);
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
