/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "min_max.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zshow.h"
#include "zwrite.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "merge_def.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "format_targ_key.h"
#include "ddphdr.h"
#include "mvalconv.h"
#include "gvcst_protos.h"	/* needed by OPEN_BASEREG_IF_STATSREG */

GBLREF int              merge_args;
GBLREF merge_glvn_ptr	mglvnp;

error_def(ERR_MERGEDESC);

/* returns 1 if no descendant issues found;
 * returns 0 if src and dst keys of merge are identical (i.e. NOOP - merge self);
 * issues MERGEDESC error otherwise.
 */
boolean_t merge_desc_check(void)
{
	boolean_t		intersect, *is_reg_in_array, mergereg_array[256];
	char			*base;
	enum db_acc_method	acc_meth1, acc_meth2;
	gd_addr			*addr;
	gd_binding		*end_map1, *end_map2, *map, *start_map1, *start_map2;
	gd_region		*reg, *reg1, *reg2;
	gv_key			*gvkey1, *gvkey2;
	gv_namehead		*gvt1, *gvt2;
	gvname_info		*gblp1, *gblp2;
	gvnh_reg_t		*gvnh_reg1, *gvnh_reg2;
	int			dollardata_src, max_fid_index;
	lv_val			*dst, *src;
	mval			tmp_mval;
	sgmnt_addrs		*csa;
	unsigned char		buff1[MAX_ZWR_KEY_SZ], buff2[MAX_ZWR_KEY_SZ], *end1, *end2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (MARG1_IS_GBL(merge_args) && MARG2_IS_GBL(merge_args))
	{
		gblp1 = mglvnp->gblp[IND1];
		gblp2 = mglvnp->gblp[IND2];
		/* Check if one global name is a descent of the other. If not, we know for sure there is no issue.
		 * If yes, further check if the database files involved in the source and target global are identical/intersect.
		 * If either of the globals span multiple regions, we need to check if the database files that the subscripted
		 * global reference (involved in the merge command) span across intersect in the source and destination globals.
		 * If intersection found issue MERGEDESC error. If not (e.g. two globals have same name but belong to different
		 * gld/db) no error needed.
		 */
		gvkey1 = gblp1->s_gv_currkey;
		gvkey2 = gblp2->s_gv_currkey;
		if (0 != memcmp(gvkey1->base, gvkey2->base, MIN(gvkey1->end, gvkey2->end)))
			return 1;
		if (gblp1->s_gd_targ_addr != gblp2->s_gd_targ_addr)
		{	/* both globals involved in the merge correspond to different gld.
			 * check if corresponding db files intersect.
			 */
			reg1 = gblp1->s_gv_cur_region;
			reg2 = gblp2->s_gv_cur_region;
			gvt1 = gblp1->s_gv_target;
			gvt2 = gblp2->s_gv_target;
			acc_meth1 = REG_ACC_METH(reg1);
			acc_meth2 = REG_ACC_METH(reg2);
			assert(!IS_ACC_METH_BG_OR_MM(acc_meth1) || (NULL != gvt1->gd_csa));
			assert(!IS_ACC_METH_BG_OR_MM(acc_meth2) || (NULL != gvt2->gd_csa));
			assert(IS_ACC_METH_BG_OR_MM(acc_meth1) || (NULL == gvt1->gd_csa));
			assert(IS_ACC_METH_BG_OR_MM(acc_meth2) || (NULL == gvt2->gd_csa));
			gvnh_reg1 = gblp1->s_gd_targ_gvnh_reg;
			gvnh_reg2 = gblp2->s_gd_targ_gvnh_reg;
			/* A non-NULL value of gvnh_reg indicates a spanning global as confirmed by the asserts below */
			assert((NULL == gvnh_reg1) || (NULL != gvnh_reg1->gvspan));
			assert((NULL == gvnh_reg2) || (NULL != gvnh_reg2->gvspan));
			if ((NULL != gvnh_reg1) || (NULL != gvnh_reg2))
			{	/* At least one of src OR dst global spans multiple regions, check for region/db intersections
				 * between the two.
				 */
				assert((NULL == gvnh_reg1) || (NULL != gvt1->gd_csa));
				assert((NULL == gvnh_reg2) || (NULL != gvt2->gd_csa));
				if ((NULL == gvt1->gd_csa) || (NULL == gvt2->gd_csa))
				{	/* One global spans multiple regions, while another is remote. They cannot intersect
					 * as a global can never span to remote regions (i.e. no dba_usr or dba_cm).
					 */
					return 1;
				}
				/* The merge command is MERGE ^gvn1(subs1)=^gvn2(subs2) where "subs1" and "subs2" are
				 * a comma-separated list of one or more subscripts. Find out regions spanned by
				 * ^gvn1(subs1) as well as ^gvn2(subs2) and check for intersections in these two lists.
				 */
				/* Find list of regions corresponding to ^gvn1(subs1) */
				base = (char *)&gvkey1->base[0];
				addr = gblp1->s_gd_targ_addr;
				/* -1 usage in "gv_srch_map" calls below is to remove trailing 0 */
				start_map1 = gv_srch_map(addr, base, gvkey1->end - 1, SKIP_BASEDB_OPEN_FALSE);
				GVKEY_INCREMENT_ORDER(gvkey1);
				end_map1 = gv_srch_map(addr, base, gvkey1->end - 1, SKIP_BASEDB_OPEN_FALSE);
				BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gvkey1->base, gvkey1->end - 1, end_map1);
				GVKEY_UNDO_INCREMENT_ORDER(gvkey1);
				/* Find list of regions corresponding to ^gvn2(subs2) */
				assert(KEY_DELIMITER == gvkey2->base[gvkey2->end - 1]);
				assert(KEY_DELIMITER == gvkey2->base[gvkey2->end]);
				base = (char *)&gvkey2->base[0];
				addr = gblp2->s_gd_targ_addr;
				start_map2 = gv_srch_map(addr, base, gvkey2->end - 1, SKIP_BASEDB_OPEN_FALSE);
				GVKEY_INCREMENT_ORDER(gvkey2);
				end_map2 = gv_srch_map(addr, base, gvkey2->end - 1, SKIP_BASEDB_OPEN_FALSE);
				BACK_OFF_ONE_MAP_ENTRY_IF_EDGECASE(gvkey2->base, gvkey2->end - 1, end_map2);
				GVKEY_UNDO_INCREMENT_ORDER(gvkey2);
				/* At this point, we are sure all regions involved in ^gvn1 and ^gvn2 are dba_mm or dba_bg.
				 * This means all the regions would have a csa and a csa->fid_index assigned to them.
				 * We can use this to determine intersections. Note though that some regions could not yet
				 * be open so we need to open the regions first in that case and then use max_fid_index.
				 */
				for (map = start_map1; map <= end_map1; map++)
				{
					OPEN_BASEREG_IF_STATSREG(map);
					reg = map->reg.addr;
					if (!reg->open)
						gv_init_reg(reg, NULL);
				}
				for (map = start_map2; map <= end_map2; map++)
				{
					OPEN_BASEREG_IF_STATSREG(map);
					reg = map->reg.addr;
					if (!reg->open)
						gv_init_reg(reg, NULL);
				}
				/* At this point, all regions involved in the merge ^gvn1(subs1)=^gvn2(subs2) are open
				 * so we can use max_fid_index without issues.
				 */
				max_fid_index = TREF(max_fid_index);
				if (max_fid_index < ARRAYSIZE(mergereg_array))
					is_reg_in_array = &mergereg_array[0];
				else
					is_reg_in_array = (boolean_t *)malloc(SIZEOF(boolean_t) * (max_fid_index + 1));
				memset(is_reg_in_array, 0, SIZEOF(boolean_t) * (max_fid_index + 1));
				intersect = FALSE;
				for (map = start_map1; map <= end_map1; map++)
				{
					reg = map->reg.addr;
					csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
					assert(NULL != csa);
					assert(max_fid_index >= csa->fid_index);
					is_reg_in_array[csa->fid_index] = TRUE;
				}
				for (map = start_map2; map <= end_map2; map++)
				{
					reg = map->reg.addr;
					csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
					assert(NULL != csa);
					assert(max_fid_index >= csa->fid_index);
					if (is_reg_in_array[csa->fid_index])
					{
						intersect = TRUE;
						break;
					}
				}
				if (is_reg_in_array != &mergereg_array[0])
					free(is_reg_in_array);
				if (!intersect)
					return 1;
			} else
			{	/* Both globals map only to one region (no spanning).
				 * if (!(both are bg/mm regions && dbs are same) &&
				 *     !(both are cm regions && on the same remote node && same region)
				 *     !(both are usr regions && in the same volume set))
				 *   NO DESCENDANTS
				 * endif
				 */
				assert((NULL == gvt1->gd_csa) || (gvt1->gd_csa != gvt2->gd_csa) || (gvt1->root == gvt2->root));
				if (!((NULL != gvt1->gd_csa) && (gvt1->gd_csa == gvt2->gd_csa))
					&& !((dba_cm == acc_meth1) && (dba_cm == acc_meth2)
						&& (reg1->dyn.addr->cm_blk == reg2->dyn.addr->cm_blk)
						&& (reg1->cmx_regnum == reg2->cmx_regnum))
					VMS_ONLY (&&
						!((dba_usr == acc_meth1) && (dba_usr == acc_meth2)
							&& ((ddp_info *)(&FILE_INFO(reg1)->file_id))->volset
								== ((ddp_info *)(&FILE_INFO(reg2)->file_id))->volset)))
				{
					UNIX_ONLY(assert((dba_usr != acc_meth1) && (dba_usr != acc_meth2));)
					return 1;
				}
			}
		} else if (gvkey1->end == gvkey2->end)
			return 0; /* NOOP - merge self */
		/* Else glds are identical and global names are identical and one is a descendant of other.
		 * So need to issue MERGEDESC error for sure (does not matter whether global spans regions
		 * or not, does not matter if region is remote or not etc.). No other checks necessary.
		 */
		if (0 == (end1 = format_targ_key(buff1, MAX_ZWR_KEY_SZ, gvkey1, TRUE)))
			end1 = &buff1[MAX_ZWR_KEY_SZ - 1];
		if (0 == (end2 = format_targ_key(buff2, MAX_ZWR_KEY_SZ, gvkey2, TRUE)))
			end2 = &buff2[MAX_ZWR_KEY_SZ - 1];
		if (gvkey1->end > gvkey2->end)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MERGEDESC, 4, end1 - buff1, buff1, end2 - buff2, buff2);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MERGEDESC, 4, end2 - buff2, buff2, end1 - buff1, buff1);
	} else if (MARG1_IS_LCL(merge_args) && MARG2_IS_LCL(merge_args))
	{
		dst = mglvnp->lclp[IND1];
		src = mglvnp->lclp[IND2];
		if ((dst == src) || (NULL == src))
		{	/* NOOP - merge self OR empty subscripted source */
			UNDO_ACTIVE_LV(actlv_merge_desc_check1); /* kill "dst" and parents as applicable if $data(dst)=0 */
			return 0;
		}
		if (lcl_arg1_is_desc_of_arg2(dst, src))
		{
			end1 = format_key_lv_val(dst, buff1, SIZEOF(buff1));
			UNDO_ACTIVE_LV(actlv_merge_desc_check2); /* kill "dst" and parents as applicable if $data(dst)=0 */
			op_fndata(src, &tmp_mval);
			dollardata_src = MV_FORCE_INTD(&tmp_mval);
			if (0 == dollardata_src)
				return 0; /* NOOP - merge x(subs)=x, but x is undefined */
			end2 = format_key_lv_val(src, buff2, SIZEOF(buff2));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MERGEDESC, 4, end1 - buff1, buff1, end2 - buff2, buff2);
		}
		if (lcl_arg1_is_desc_of_arg2(src, dst))
		{
			/* No need of UNDO_ACTIVE_LV since src is a descendant of dst and so $data(dst) is != 0 */
			end1 = format_key_lv_val(dst, buff1, SIZEOF(buff1));
			end2 = format_key_lv_val(src, buff2, SIZEOF(buff2));
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MERGEDESC, 4, end2 - buff2, buff2, end1 - buff1, buff1);
		}
		if (LV_IS_BASE_VAR(src))
		{	/* source is unsubscripted. check if source is empty */
			op_fndata(src, &tmp_mval);
			dollardata_src = MV_FORCE_INTD(&tmp_mval);
			if (0 == dollardata_src)
			{	/* NOOP - merge with empty unsubscripted source local variable */
				UNDO_ACTIVE_LV(actlv_merge_desc_check3); /* kill "dst" and parents as applicable if $data(dst)=0 */
				return 0;
			}
		}
	}
	return 1;
}
