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

/*
 * ------------------------------------------------------------------------------------------------------------
 * op_merge.c
 * ==============
 * Description:
 *  	Main routine of MERGE command.
 *
 * Arguments:
 *	Already op_merge_arg.c have saved all the inputs in merge_args and mglvnp.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	merge_args will be reset to 0 after successful operation.
 *
 * Notes:
 * ------------------------------------------------------------------------------------------------------------
 */
#include "mdef.h"

#include "min_max.h"
#include "lv_val.h"
#include <rtnhdr.h>
#include "mv_stent.h"
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"
#include "tp.h"
#include "gtm_string.h"
#include "merge_def.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "gvsub2str.h"
#include "op.h"
#include "mvalconv.h"
#include "stringpool.h"
#include "outofband.h"
#include "gtmmsg.h"
#include "format_targ_key.h"
#include "sgnl.h"
#include "util.h"
#include "collseq.h"
#include "alias.h"

#define UNDO_ACTIVE_LV								\
{										\
	if (NULL != active_lv)							\
	{									\
		if (!LV_IS_VAL_DEFINED(active_lv) && !LV_HAS_CHILD(active_lv))	\
			op_kill(active_lv);					\
		active_lv = (lv_val *)NULL;					\
	}									\
}

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF int4             outofband;
GBLREF uint4		dollar_tlevel;
GBLREF gv_key           *gv_currkey;
GBLREF gd_region        *gv_cur_region;
GBLREF zshow_out        *zwr_output;
GBLREF int              merge_args;
GBLREF merge_glvn_ptr	mglvnp;
GBLREF gv_namehead      *gv_target;
GBLREF lvzwrite_datablk *lvzwrite_block;
GBLREF lv_val		*active_lv;

error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_MERGEINCOMPL);
error_def(ERR_NCTCOLLDIFF);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void op_merge(void)
{
	boolean_t		found, check_for_null_subs, is_base_var, nontp_and_bgormm;
	lv_val			*dst_lv;
	mval 			*mkey, *value, *subsc;
	int			org_glvn1_keysz, org_glvn2_keysz, delta2, dollardata_src, dollardata_dst, sbs_depth;
	unsigned char		*ptr, *ptr2;
	unsigned char  		buff[MAX_ZWR_KEY_SZ];
	unsigned char		nullcoll_src, nullcoll_dst;
	zshow_out		output;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_STRLEN >= MAX_ZWR_KEY_SZ);
	assert ((merge_args == (MARG1_LCL | MARG2_LCL)) ||
		(merge_args == (MARG1_LCL | MARG2_GBL)) ||
		(merge_args == (MARG1_GBL | MARG2_LCL)) ||
		(merge_args == (MARG1_GBL | MARG2_GBL)));
	assert(!lvzwrite_block || 0 == lvzwrite_block->curr_subsc);
	/* Need to protect value from stpgcol */
	PUSH_MV_STENT(MVST_MVAL);
	value = &mv_chain->mv_st_cont.mvs_mval;
	value->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before value gets initialized below */
	if (MARG2_IS_GBL(merge_args))
	{	/* Need to protect mkey returned from gvcst_queryget from stpgcol */
		PUSH_MV_STENT(MVST_MVAL);
		mkey = &mv_chain->mv_st_cont.mvs_mval;
		mkey->mvtype = 0; /* initialize mval in M-stack in case stp_gcol gets called before mkey gets initialized below */
		gvname_env_restore(mglvnp->gblp[IND2]);
		/* now $DATA will be done for gvn2. op_gvdata input parameters are set in the form of some GBLREF */
		op_gvdata(value);
		dollardata_src = MV_FORCE_INT(value);
		if (0 == dollardata_src)
		{	/* nothing in source global */
			UNDO_ACTIVE_LV;
			POP_MV_STENT();	/* value */
			POP_MV_STENT(); /* mkey */
			if (MARG1_IS_GBL(merge_args))
				gvname_env_restore(mglvnp->gblp[IND1]);	 /* store destination as naked indicator in gv_currkey */
			merge_args = 0;	/* Must reset to zero to reuse the Global */
			return;
		}
		nontp_and_bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
				&& !dollar_tlevel;
		assert(!nontp_and_bgormm || gv_target->root);
		if (NULL == TREF(gv_mergekey2))
		{	/* We need to initialize gvn2 (right hand side). */
			GVKEY_INIT(TREF(gv_mergekey2), DBKEYSIZE(MAX_KEY_SZ));
		}
		org_glvn1_keysz = mglvnp->gblp[IND1]->s_gv_currkey->end + 1;
		org_glvn2_keysz = gv_currkey->end + 1;
		(TREF(gv_mergekey2))->end = gv_currkey->end;
		(TREF(gv_mergekey2))->prev = gv_currkey->prev;
		memcpy((TREF(gv_mergekey2))->base, gv_currkey->base, gv_currkey->end + 1);
		if (MARG1_IS_GBL(merge_args))
		{	/*==================== MERGE ^gvn1=^gvn2 =====================*/
			if (mglvnp->gblp[IND2]->s_gv_target->nct != mglvnp->gblp[IND1]->s_gv_target->nct)
				rts_error(VARLSTCNT(1) ERR_NCTCOLLDIFF);
			/* if self merge then NOOP*/
			if (!merge_desc_check()) /* will not proceed if one is descendant of another */
			{
				gvname_env_restore(mglvnp->gblp[IND1]);	 /* store destination as naked indicator in gv_currkey */
				POP_MV_STENT();	/* value */
				merge_args = 0;	/* Must reset to zero to reuse the Global */
				return;
			}
			nullcoll_src = mglvnp->gblp[IND2]->s_gv_cur_region->std_null_coll;
			nullcoll_dst = mglvnp->gblp[IND1]->s_gv_cur_region->std_null_coll;
			if (1 == dollardata_src || 11 == dollardata_src)
			{
				found = op_gvget(value);  /* value of ^glvn2 */
				if (found)
				{	/* SET ^gvn1=^gvn2 */
					gvname_env_restore(mglvnp->gblp[IND1]);
					op_gvput(value);
					/* Note: If ^gvn1's null_sub=ALLOWEXISTING and say ^gvn1("")=^gvn,
					 * this will give NULL_SUBC error
					 */
				}
			}
			check_for_null_subs = (NEVER != mglvnp->gblp[IND2]->s_gv_cur_region->null_subs) &&
				(ALWAYS != mglvnp->gblp[IND1]->s_gv_cur_region->null_subs);
			/* Traverse descendant of ^gvn2 and copy into ^gvn1 */
			for (; ;)
			{
				if (outofband)
				{
					gvname_env_restore(mglvnp->gblp[IND1]); /* naked indicator is restored into gv_currkey */
					outofband_action(FALSE);
				}
				/* Restore last key under ^gvn2 we worked */
				gvname_env_restore(mglvnp->gblp[IND2]);
				assert(0 == gv_currkey->base[gv_currkey->end - 1] && 0 == gv_currkey->base[gv_currkey->end]);
				/* following is an attempt to find immidiate right sibling */
				gv_currkey->base[gv_currkey->end] = 1;
				gv_currkey->base[gv_currkey->end + 1] = 0;
				gv_currkey->base[gv_currkey->end + 2] = 0;
				gv_currkey->end += 2;
#				ifdef UNIX
				if (nontp_and_bgormm && (0 == gv_target->root))
				{	/* This is to handle root blocks moved by REORG. Merge alternates between two
					 * different gv_targets. If op_gvput below detects a moved root block, it will
					 * set the root of both to zero, but it will only redo root search for IND1.
					 * We want op_gvqueryget to redo root search for IND2, so we fake its root
					 * which will get it far enough to do so.
					 */
					cs_addrs->root_search_cycle--;
					gv_target->root = 2;
				}
#				endif
				/* Do atomic $QUERY and $GET of current glvn2:
				 * mkey is a mstr which contains $QUERY result in database format (So no conversion necessary)
				 * value is a mstr which contains $GET result
				 */
				if (!op_gvqueryget(mkey, value))
					break;
				assert(MV_IS_STRING(mkey));
				if (mkey->str.len < org_glvn2_keysz)
					break;
				if (0 != *((unsigned char *)mkey->str.addr + (TREF(gv_mergekey2))->end - 1) ||
					memcmp(mkey->str.addr, (TREF(gv_mergekey2))->base, (TREF(gv_mergekey2))->end - 1))
					break; 					/* mkey is not under the sub-tree */
				delta2 = mkey->str.len - org_glvn2_keysz; 	/* length increase of source key */
				assert (0 < delta2);
				/* Save the new source key for next iteration */
				memcpy(mglvnp->gblp[IND2]->s_gv_currkey->base + org_glvn2_keysz - 2,
					mkey->str.addr + org_glvn2_keysz - 2, delta2 + 2);
				mglvnp->gblp[IND2]->s_gv_currkey->end = mkey->str.len - 1;
				/* Create the destination key for this iteration (under ^glvn1) */
				gvname_env_restore(mglvnp->gblp[IND1]);
				if (gv_cur_region->max_key_size < org_glvn1_keysz + delta2)
					ISSUE_GVSUBOFLOW_ERROR(gv_currkey);
				assert(gv_currkey->end == org_glvn1_keysz - 1);
				memcpy(gv_currkey->base + org_glvn1_keysz - 2,
					mkey->str.addr + org_glvn2_keysz - 2, delta2 + 2);
				gv_currkey->end = org_glvn1_keysz + delta2 - 1;
				if (nullcoll_src != nullcoll_dst)
				{
					if (0 == nullcoll_dst)
					{	/* Standard to GTM null subscript conversion*/
						STD2GTMNULLCOLL((unsigned char *)gv_currkey->base + org_glvn1_keysz - 1,
								delta2 - 1);
					} else
					{	/*  GTM to standard null subscript conversion */
						GTM2STDNULLCOLL((unsigned char *)gv_currkey->base + org_glvn1_keysz - 1,
								delta2 - 1);
					}
				}
				/* check null subscripts in destination key, note that we have already restored, destination global
				 * and curresponding region, key information
				 */
				if (check_for_null_subs)
				{
					ptr2 = gv_currkey->base + gv_currkey->end - 1;
					for (ptr = gv_currkey->base + org_glvn1_keysz - 2; ptr < ptr2; )
					{
						if (KEY_DELIMITER == *ptr++ && KEY_DELIMITER == *(ptr + 1) &&
							(0 == gv_cur_region->std_null_coll ? (STR_SUB_PREFIX == *ptr) :
							(SUBSCRIPT_STDCOL_NULL == *ptr)))
							/* Note: For sgnl_gvnulsubsc/rts_error
							 * 	 we do not restore proper naked indicator.
							 * The standard states that the effect of a MERGE command
							 * on the naked indicator is that the naked indicator will be changed
							 * as if a specific SET command would have been executed.
							 * The standard also states that the effect on the naked indicator
							 * will only take be visible after the MERGE command has completed.
							 * So, if there is an error during the execution of a MERGE command,
							 * the standard allows the naked indicator to reflect any intermediate
							 * state. This provision was made intentionally, otherwise it would
							 * have become nearly impossible to create a fully standard
							 * implementation. : From Ed de Moel : 2/1/2
							 */
							sgnl_gvnulsubsc();
					}
				}
				/* Now put value of ^glvn2 descendant into corresponding descendant under ^glvn1 */
				op_gvput(value);
			}
			gvname_env_restore(mglvnp->gblp[IND1]);	 /* store destination as naked indicator in gv_currkey */
		} else
		{	/*==================== MERGE lvn1=^gvn2 =====================*/
			assert(MARG1_IS_LCL(merge_args));
			assert(mglvnp->lclp[IND1]);
			/* Need to protect subsc created from global variable subscripts from stpgcol */
			PUSH_MV_STENT(MVST_MVAL);
			subsc = &mv_chain->mv_st_cont.mvs_mval;
			/* Restore ^gvn2 we will work */
			gvname_env_save(mglvnp->gblp[IND2]);
			if (1 == dollardata_src || 11 == dollardata_src)
			{	/* SET lvn1=^gvn2 */
				found = op_gvget(value);
				if (found)
					mglvnp->lclp[IND1]->v = *value;
			}
			for (; ;)
			{
				if (outofband)
				{
					gvname_env_restore(mglvnp->gblp[IND2]);	 /* naked indicator is restored into gv_currkey */
					outofband_action(FALSE);
				}
				assert(0 == gv_currkey->base[gv_currkey->end - 1] && 0 == gv_currkey->base[gv_currkey->end]);
				/* following is an attempt to find immidiate right sibling */
				gv_currkey->base[gv_currkey->end] = 1;
				gv_currkey->base[gv_currkey->end + 1] = 0;
				gv_currkey->base[gv_currkey->end + 2] = 0;
				gv_currkey->end += 2;
				/* Do $QUERY and $GET of current glvn2. Result will be in mkey and value respectively.
				 * mkey->str contains data as database format. So no conversion necessary
				 */
				if (!op_gvqueryget(mkey, value))
					break;
				if (mkey->str.len < (TREF(gv_mergekey2))->end + 1)
					break;
				ptr = (unsigned char *)mkey->str.addr +  (TREF(gv_mergekey2))->end - 1;
				if (0 != *ptr || memcmp(mkey->str.addr, (TREF(gv_mergekey2))->base, (TREF(gv_mergekey2))->end - 1))
					break;
				assert(MV_IS_STRING(mkey));
				delta2 = mkey->str.len - org_glvn2_keysz; /* length increase of key */
				assert (0 < delta2);
				/* Create next key for ^glvn2 */
				memcpy(gv_currkey->base + org_glvn2_keysz - 2, mkey->str.addr + org_glvn2_keysz - 2, delta2 + 2);
				gv_currkey->end = mkey->str.len - 1;
				/* Now add subscripts to create the entire key */
				dst_lv =  mglvnp->lclp[IND1];
				is_base_var = LV_IS_BASE_VAR(dst_lv);
				ptr = (unsigned char *)gv_currkey->base + org_glvn2_keysz - 1;
				assert(*ptr);
				do
				{
					LV_SBS_DEPTH(dst_lv, is_base_var, sbs_depth);
					if (MAX_LVSUBSCRIPTS <= sbs_depth)
						rts_error(VARLSTCNT(3) ERR_MERGEINCOMPL, 0, ERR_MAXNRSUBSCRIPTS);
					ptr2 = gvsub2str(ptr, buff, FALSE);
					subsc->mvtype = MV_STR;
					subsc->str.addr = (char *)buff;
					subsc->str.len = INTCAST(ptr2 - buff);
					s2pool(&subsc->str);
					dst_lv = op_putindx(VARLSTCNT(2) dst_lv, subsc);
					while (*ptr++);	/* skip to start of next subscript */
					is_base_var = FALSE;
				} while (*ptr);
				/* We created the key. Pre-process the node in case a container is being replaced,
				 * then assign the value directly. Note there is no need to worry about MV_ALIASCONT
				 * propagation since the source in this case is a global var.
				 */
				DECR_AC_REF(dst_lv, TRUE);
				dst_lv->v = *value;
			}
			gvname_env_restore(mglvnp->gblp[IND2]);	 /* naked indicator is restored into gv_currkey */
			POP_MV_STENT();     /* subsc */
		}
		POP_MV_STENT();     /* mkey */
	} else
	{	/* source is local */
		op_fndata(mglvnp->lclp[IND2], value);
		dollardata_src = MV_FORCE_INT(value);
		if (0 == dollardata_src)
		{
			UNDO_ACTIVE_LV;
			POP_MV_STENT();	/* value */
			if (MARG1_IS_GBL(merge_args))
				gvname_env_restore(mglvnp->gblp[IND1]);	 /* store destination as naked indicator in gv_currkey */
			merge_args = 0;	/* Must reset to zero to reuse the Global */
			return;
		}
		/* not memsetting output to 0 here can cause garbage value of output.out_var.lv.child which in turn can
		 * cause a premature return from lvzwr_var resulting in op_merge() returning without having done the merge.
		 */
		memset(&output, 0, SIZEOF(output));
		if (MARG1_IS_LCL(merge_args))
		{	/*==================== MERGE lvn1=lvn2 =====================*/
			assert(mglvnp->lclp[IND1]);
			/* if self merge then NOOP */
			if (!merge_desc_check()) /* will not proceed if one is descendant of another */
			{
				POP_MV_STENT();	/* value */
				merge_args = 0;	/* Must reset to zero to reuse the Global */
				return;
			}
			output.buff = (char *)buff;
			output.ptr = output.buff;
			output.out_var.lv.lvar = mglvnp->lclp[IND1];
			zwr_output = &output;
			lvzwr_init(zwr_patrn_mident, &mglvnp->lclp[IND2]->v);
			lvzwr_arg(ZWRITE_ASTERISK, 0, 0);
			lvzwr_var(mglvnp->lclp[IND2], 0);
			/* assert that destination got all data of the source and its descendants */
			DEBUG_ONLY(op_fndata(mglvnp->lclp[IND1], value));
			DEBUG_ONLY(dollardata_dst = MV_FORCE_INT(value));
			assert((dollardata_src & dollardata_dst) == dollardata_src);
		} else
		{	/*==================== MERGE ^gvn1=lvn2 =====================*/
			assert(MARG1_IS_GBL(merge_args) && MARG2_IS_LCL(merge_args));
			gvname_env_save(mglvnp->gblp[IND1]);
			output.buff = (char *)buff;
			output.ptr = output.buff;
			output.out_var.gv.end = gv_currkey->end;
			output.out_var.gv.prev = gv_currkey->prev;
			zwr_output = &output;
			lvzwr_init(zwr_patrn_mident, &mglvnp->lclp[IND2]->v);
			lvzwr_arg(ZWRITE_ASTERISK, 0, 0);
			lvzwr_var(mglvnp->lclp[IND2], 0);
			gvname_env_restore(mglvnp->gblp[IND1]);	 /* store destination as naked indicator in gv_currkey */
		}
	}
	POP_MV_STENT();	/* value */
	merge_args = 0;	/* Must reset to zero to reuse the Global */
}
