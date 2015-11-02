/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "lv_val.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "mlkdef.h"
#include "zshow.h"
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
#include "op.h"
#include "gtmmsg.h"
#include "sgnl.h"
#include "stringpool.h"
#include "alias.h"
#include "callg.h"

GBLREF lvzwrite_datablk	*lvzwrite_block;
GBLREF zshow_out	*zwr_output;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF int		merge_args;
GBLREF merge_glvn_ptr	mglvnp;
GBLREF gd_region	*gv_cur_region;
GBLREF symval		*curr_symval;
GBLREF zwr_hash_table	*zwrhtab;			/* Used to track aliases during zwrites */
GBLREF uint4		zwrtacindx;			/* When creating $ZWRTACxxx vars for ZWRite, this holds xxx */

LITDEF MSTR_CONST(semi_star, " ;*");
LITDEF MSTR_CONST(dzwrtac_clean, "$ZWRTAC=\"\"");

error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_MERGEINCOMPL);

void lvzwr_out_targkey(mstr *one);


void lvzwr_out_targkey(mstr *one)
{
	int	n, nsubs;

	zshow_output(zwr_output, lvzwrite_block->curr_name);
	nsubs = lvzwrite_block->curr_subsc;
	if (nsubs)
	{
		*one->addr = '(';
		zshow_output(zwr_output, one);
		for (n = 0 ; ; )
		{
			mval_write(zwr_output, ((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[n].actual, FALSE);
			if (++n < nsubs)
			{
				*one->addr = ',';
				zshow_output(zwr_output, one);
			} else
			{
				*one->addr = ')';
				zshow_output(zwr_output, one);
				break;
			}
		}
	}
}


void lvzwr_out(lv_val *lvp)
{
	char 			buff;
	uchar_ptr_t		lastc;
	int			n, nsubs, sbs_depth;
	lv_val			*dst_lv, *res_lv, *lvpc;
	mstr 			one;
	mval 			*subscp, *val, outindx;
	ht_ent_addr		*tabent_addr;
	ht_ent_mname		*tabent_mname;
	boolean_t		htent_added, dump_container;
	zwr_alias_var		*newzav, *zav;
	mident_fixed		zwrt_varname;
	lvzwrite_datablk	*newzwrb;
	gparam_list		param_list;	/* for op_putindx call through callg */

	val = &lvp->v;
	assert(lvzwrite_block);
	if (!merge_args)
	{	/* The cases that exist here are:
		 * 1) This is a container variable. If the lv_val it refers to has been printed, show that association.
		 *    Else, "create" a $ZWRTACxxx var/index that will define the value. Then before returning, cause
		 *    that container var to be dumped with the appropriate $ZWRTACxxx index as the var name.
		 * 2) This is an alias base variable. If first time seen, we print normally but record it and put a
		 *    ";#" tag on the end to signify it is an alias var (doesn't affect value). If we look it up and it
		 *    is not the first time this lv_val has been printed, then we instead print the statement needed to
		 *    alias it to the first seen var.
		 * 3) This is just a normal var needing to be printed normally.
		 */
		htent_added = FALSE;
		one.addr = &buff;
		one.len = 1;
		lvzwrite_block->zav_added = FALSE;
		if (lvp->v.mvtype & MV_ALIASCONT)
		{	/* Case 1 -- have an alias container */
			assert(curr_symval->alias_activity);
			assert(!LV_IS_BASE_VAR(lvp));	/* verify is subscripted var */
			lvpc = (lv_val *)lvp->v.str.addr;
			assert(lvpc);
			assert(LV_IS_BASE_VAR(lvpc));	/* Verify base var lv_val */
			if (tabent_addr = (ht_ent_addr *)lookup_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lvpc))
			{	/* The value was found, we have  a reference we can print now */
				assert(HTENT_VALID_ADDR(tabent_addr, zwr_alias_var, zav));
				*one.addr = '*';
				zshow_output(zwr_output, &one);
				lvzwr_out_targkey(&one);
				*one.addr = '=';
				zshow_output(zwr_output, &one);
				zav = (zwr_alias_var *)tabent_addr->value;
				assert(0 < zav->zwr_var.len);
				zwr_output->flush = TRUE;
				zshow_output(zwr_output, (const mstr *)&zav->zwr_var);
				return;
			}
			/* This lv_val isn't known to us yet. Scan the hash curr_symval hash table to see if it is known as a
			 * base variable as we could have a "forward reference" here.
			 */
			tabent_mname = als_lookup_base_lvval(lvpc);
			/* note even though both paths below add a zav, not bothering to set zav_added because that flag is
			 * really only (currently) cared about in reference to processing a basevar so we wouldn't
			 * be in this code path anyway. Comment here to record potential usage if that changes.
			 */
			if (tabent_mname)
			{	/* Found a base var it can reference -- create a zwrhtab entry for it */
				assert(tabent_mname->key.var_name.len);
				newzav = als_getzavslot();
				newzav->zwr_var = tabent_mname->key.var_name;
				htent_added = add_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lvpc, newzav, &tabent_addr);
				assert(htent_added);
				dump_container = FALSE;
			} else
			{	/* Unable to find lv_val .. must be "orphaned" so we generate a new $ZWRTAC var for it. The first
				 * check however is if this is the first $ZWRTAC var being generated for this $ZWR. If yes, generate
				 * a $ZWRTAC="" line to preceed it. This will be a flag to load to clear out all existing $ZWRTAC
				 * temp vars so there is no pollution between loads of ZWRitten data.
				 */
				if (0 == zwrtacindx++)
				{	/* Put out "dummy" statement that will clear all the $ZWRTAC vars for a clean slate */
					zwr_output->flush = TRUE;
					zshow_output(zwr_output, &dzwrtac_clean);
				}
				MEMCPY_LIT(zwrt_varname.c, DOLLAR_ZWRTAC);
				lastc = i2asc((uchar_ptr_t)zwrt_varname.c + STR_LIT_LEN(DOLLAR_ZWRTAC), zwrtacindx);
				newzav =  als_getzavslot();
				newzav->zwr_var.addr = zwrt_varname.c;
				newzav->zwr_var.len = INTCAST(((char *)lastc - &zwrt_varname.c[0]));
				s2pool(&newzav->zwr_var);
				htent_added = add_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lvpc, newzav, &tabent_addr);
				assert(htent_added);
				dump_container = TRUE;
			}
			/* Note value_printed flag in newzav not set since we are NOT dumping the value at this point
			 * but only the association. Since the flag is not set, we *will* dump it when we get to that
			 * actual variable.
			 */
			*one.addr = '*';
			zshow_output(zwr_output, &one);
			lvzwr_out_targkey(&one);
			*one.addr = '=';
			zshow_output(zwr_output, &one);
			zwr_output->flush = TRUE;
			zshow_output(zwr_output, (const mstr *)&newzav->zwr_var);
			if (dump_container)
			{	/* We want to dump the entire container variable but the name doesn't match the var we are
				 * currently dumping so push a new lvzwrite_block onto the stack, fill it in for the current var
				 * and call lvzwr_var() to handle it. When done, dismantle the temp lvzwrite_block.
				 */
				newzwrb = (lvzwrite_datablk *)malloc(SIZEOF(lvzwrite_datablk));
				memset(newzwrb, 0, SIZEOF(lvzwrite_datablk));
				newzwrb->sub = (zwr_sub_lst *)malloc(SIZEOF(zwr_sub_lst) * MAX_LVSUBSCRIPTS);
				newzwrb->curr_name = &newzav->zwr_var;
				newzwrb->prev = lvzwrite_block;
				lvzwrite_block = newzwrb;
				lvzwr_var(lvpc, 0);
				assert(newzav->value_printed);
				assert(newzwrb == lvzwrite_block);
				free(newzwrb->sub);
				lvzwrite_block = newzwrb->prev;
				free(newzwrb);
			}
			return;
		} else if (LV_IS_BASE_VAR(lvp) && IS_ALIASLV(lvp))
		{	/* Case 2 -- alias base variable (only base vars have reference counts). Note this can occur with
			 * TP save/restore vars since we increment both trefcnt and crefcnt for these hidden copied references.
			 * Because of that, we can't assert alias_activity but otherwise it shouldn't affect processing.
			 */
			if (!(htent_added = add_hashtab_addr(&zwrhtab->h_zwrtab, (char **)&lvp, NULL, &tabent_addr)))
			{	/* Entry already existed -- need to output association rather than values */
				assert(tabent_addr);
				zav = (zwr_alias_var *)tabent_addr->value;
				assert(zav);
				if (zav->value_printed)
				{	/* Value has already been output -- print association this time */
					*one.addr = '*';	/* Flag as creating an alias */
					zshow_output(zwr_output, &one);
					/* Now for (new) variable name */
					zshow_output(zwr_output, lvzwrite_block->curr_name);
					*one.addr = '=';
					zshow_output(zwr_output, &one);
					/* .. and the var name aliasing to (the first seen with this lv_val) */
					assert(zav->zwr_var.len);
					zwr_output->flush = TRUE;
					zshow_output(zwr_output, &zav->zwr_var);
					return;
				}
				/* Else the value for this entry has not yet been printed so let us fall into case 3
				 * and get that done. Also set the flag so we mark it as an alias. Note this can happen if
				 * a container value for a name is encountered before the base var it points to. We will
				 * properly resolve the entry but its value  won't have been printed until we actually encounter
				 * it in the tree.
				 */
				htent_added = TRUE;		/* to force the ;# tag at end of value printing */
				zav->value_printed = TRUE;	/* value will be output shortly below */
			} else
			{	/* Entry was added so is first appearance -- give it a value to hold onto and print it */
				newzav = als_getzavslot();
				newzav->zwr_var = *lvzwrite_block->curr_name;
				newzav->value_printed = TRUE;		/* or rather it will be shortly.. */
				tabent_addr->value = (void *)newzav;
				lvzwrite_block->zav_added = TRUE;
				/* Note fall into case 3 to print var and value if exists */
			}
		}
		/* Case 3 - everything else */
		if (!MV_DEFINED(val))
			return;
		MV_FORCE_STR(val);
		lvzwr_out_targkey(&one);
		*one.addr = '=';
		zshow_output(zwr_output, &one);
		mval_write(zwr_output, val, !htent_added);
		if (htent_added)
		{	/* output the ";#" tag to indicate this is an alias output */
			zwr_output->flush = TRUE;
			zshow_output(zwr_output, &semi_star);
		}
	} else
	{	/* MERGE assignment from local variable */
		nsubs = lvzwrite_block->curr_subsc;
		if (MARG1_IS_GBL(merge_args))
		{	/* Target global var */
			memcpy(gv_currkey->base, mglvnp->gblp[IND1]->s_gv_currkey->base, mglvnp->gblp[IND1]->s_gv_currkey->end + 1);
			gv_currkey->end = mglvnp->gblp[IND1]->s_gv_currkey->end;
			for (n = 0 ; n < nsubs; n++)
			{
				subscp = ((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[n].actual;
				MV_FORCE_STR(subscp);
				mval2subsc(subscp, gv_currkey);
				if (!subscp->str.len &&	(ALWAYS != gv_cur_region->null_subs))
					sgnl_gvnulsubsc();
			}
			MV_FORCE_STR(val);
			op_gvput(val);
		} else
		{	/* Target local var - pre-process target in case is a container */
			assert(MARG1_IS_LCL(merge_args));
			dst_lv = mglvnp->lclp[IND1];
			if (!LV_IS_BASE_VAR(dst_lv))
			{
				LV_SBS_DEPTH(dst_lv, FALSE, sbs_depth);
				if (MAX_LVSUBSCRIPTS < (sbs_depth + nsubs))
					rts_error(VARLSTCNT(3) ERR_MERGEINCOMPL, 0, ERR_MAXNRSUBSCRIPTS);
			}
			param_list.arg[0] = dst_lv;	/* this is already protected from stp_gcol by op_merge so no need to
							 * push this into the stack for stp_gcol protection. */
			for (n = 0 ; n < nsubs; n++)
			{	/* Note: no need to do push these mvals on the stack before calling op_putindx
				 * as lvzwrite_block->sub is already protected by stp_gcol_src.h.
				 */
				param_list.arg[n+1] = ((zwr_sub_lst *)lvzwrite_block->sub)->subsc_list[n].actual;
			}
			param_list.n = n + 1;
			dst_lv = (lv_val *)callg((callgfnptr)op_putindx, &param_list);
			MV_FORCE_STR(val);
			DECR_AC_REF(dst_lv, TRUE);
			dst_lv->v = *val;
			dst_lv->v.mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
		}
	}
}
