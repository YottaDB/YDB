/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "zyencode_zydecode_def.h"
#include "op_zyencode_zydecode.h"
#include "format_targ_key.h"
#include "gvcst_protos.h"	/* needed by OPEN_BASEREG_IF_STATSREG */

GBLREF int			zydecode_args;
GBLREF int			zyencode_args;
GBLREF zydecode_glvn_ptr	dglvnp;
GBLREF zyencode_glvn_ptr	eglvnp;

/* Returns if no descendant issues found,
 * or issues ERR_ZYENCODEDESC or ERR_ZYDECODEDESC error otherwise.
 */
void zyencode_zydecode_desc_check(int desc_error)
{
	boolean_t		intersect, *is_reg_in_array, reg_array[256];
	char			*base;
	enum db_acc_method	acc_meth1, acc_meth2;
	gd_addr			*addr;
	gd_binding		*end_map1, *end_map2, *map, *start_map1, *start_map2;
	gd_region		*reg, *reg1, *reg2;
	gv_key			*gvkey1, *gvkey2;
	gv_namehead		*gvt1, *gvt2;
	gvname_info		*gblp1, *gblp2;
	gvnh_reg_t		*gvnh_reg1, *gvnh_reg2;
	lvname_info		lvn_info;
	int			max_fid_index, args;
	lv_val			*dst, *src;
	mval			subsc_arr[MAX_LVSUBSCRIPTS];
	sgmnt_addrs		*csa;
	unsigned char		buff1[MAX_ZWR_KEY_SZ], buff2[MAX_ZWR_KEY_SZ], *end1, *end2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args = (ERR_ZYENCODEDESC == desc_error) ? zyencode_args : zydecode_args;
	if (ARG1_IS_GBL(args) && ARG2_IS_GBL(args))
	{
		gblp1 = (ERR_ZYENCODEDESC == desc_error) ? eglvnp->gblp[IND1] : dglvnp->gblp[IND1];
		gblp2 = (ERR_ZYENCODEDESC == desc_error) ? eglvnp->gblp[IND2] : dglvnp->gblp[IND2];
		/* Check if one global name is a descendant of the other. If not, we know for sure there is no issue.
		 * If yes, further check if the database files involved in the source and target global are identical/intersect.
		 * If either of the globals span multiple regions, we need to check if the database files that the subscripted
		 * global reference (involved in the command) span across intersect in the source and destination globals.
		 * If intersection found issue ERR_ZYENCODEDESC or ERR_ZYDECODEDESC error. If not (e.g. two globals have same
		 * name but belong to different gld/db) no error needed.
		 */
		gvkey1 = gblp1->s_gv_currkey;
		gvkey2 = gblp2->s_gv_currkey;
		if (0 != memcmp(gvkey1->base, gvkey2->base, MIN(gvkey1->end, gvkey2->end)))
			return;
		if (gblp1->s_gd_targ_addr != gblp2->s_gd_targ_addr)
		{	/* both globals involved in the command correspond to different gld.
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
					 * as a global can never span to remote regions (i.e. no dba_cm).
					 */
					return;
				}
				/* The command is ZYENCODE/ZYDECODE ^gvn1(subs1)=^gvn2(subs2) where "subs1" and "subs2" are
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
						gv_init_reg(reg);
				}
				for (map = start_map2; map <= end_map2; map++)
				{
					OPEN_BASEREG_IF_STATSREG(map);
					reg = map->reg.addr;
					if (!reg->open)
						gv_init_reg(reg);
				}
				/* At this point, all regions involved in the zyencode/zydecode ^gvn1(subs1)=^gvn2(subs2)
				 * are open so we can use max_fid_index without issues.
				 */
				max_fid_index = TREF(max_fid_index);
				if (max_fid_index < ARRAYSIZE(reg_array))
					is_reg_in_array = &reg_array[0];
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
				if (is_reg_in_array != &reg_array[0])
					free(is_reg_in_array);
				if (!intersect)
					return;
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
						&& (reg1->cmx_regnum == reg2->cmx_regnum)))
				{
					UNIX_ONLY(assert((dba_usr != acc_meth1) && (dba_usr != acc_meth2));)
					return;
				}
			}
		}
		/* Else glds are identical and global names are identical and one is a descendant of other.
		 * So need to issue ERR_ZYENCODEDESC or ERR_ZYDECODEDESC error for sure (does not matter whether global
		 * spans regions or not, does not matter if region is remote or not etc.). No other checks necessary.
		 */
		if (0 == (end1 = format_targ_key(buff1, MAX_ZWR_KEY_SZ, gvkey1, TRUE)))
			end1 = &buff1[MAX_ZWR_KEY_SZ - 1];
		if (0 == (end2 = format_targ_key(buff2, MAX_ZWR_KEY_SZ, gvkey2, TRUE)))
			end2 = &buff2[MAX_ZWR_KEY_SZ - 1];
		if (ERR_ZYENCODEDESC == desc_error)
			zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
		else
			zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
		if (gvkey1->end > gvkey2->end)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) desc_error, 4, end1 - buff1, buff1, end2 - buff2, buff2);
		else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) desc_error, 4, end2 - buff2, buff2, end1 - buff1, buff1);
	} else if (ARG1_IS_LCL(args) && ARG2_IS_LCL(args))
	{
		dst = (ERR_ZYENCODEDESC == desc_error) ? eglvnp->lclp[IND1] : dglvnp->lclp[IND1];
		src = (ERR_ZYENCODEDESC == desc_error) ? eglvnp->lclp[IND2] : dglvnp->lclp[IND2];
		if ((dst == src) || lcl_arg1_is_desc_of_arg2(dst, src))
		{
			BUILD_FORMAT_KEY_MVALS(dst, subsc_arr, &lvn_info);
			end1 = format_key_mvals(buff1, SIZEOF(buff1), &lvn_info);
			BUILD_FORMAT_KEY_MVALS(src, subsc_arr, &lvn_info);
			end2 = format_key_mvals(buff2, SIZEOF(buff2), &lvn_info);
			UNDO_ACTIVE_LV(actlv_zyen_zyde_desc_check2); /* kill "dst" and parents as applicable if $data(dst)=0 */
			if (ERR_ZYENCODEDESC == desc_error)
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
			else
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) desc_error, 4, end1 - buff1, buff1, end2 - buff2, buff2);
		}
		if (lcl_arg1_is_desc_of_arg2(src, dst))
		{
			/* No need of UNDO_ACTIVE_LV since src is a descendant of dst and so $data(dst) is != 0 */
			BUILD_FORMAT_KEY_MVALS(dst, subsc_arr, &lvn_info);
			end1 = format_key_mvals(buff1, SIZEOF(buff1), &lvn_info);
			BUILD_FORMAT_KEY_MVALS(src, subsc_arr, &lvn_info);
			end2 = format_key_mvals(buff2, SIZEOF(buff2), &lvn_info);
			if (ERR_ZYENCODEDESC == desc_error)
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
			else
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) desc_error, 4, end2 - buff2, buff2, end1 - buff1, buff1);
		}
	}
	return;
}
