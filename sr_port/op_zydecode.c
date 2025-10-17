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

/*
 * ------------------------------------------------------------------------------------------------------------
 * op_zydecode.c
 * ==============
 * Description:
 *	Main routine of ZYDECODE command.
 *
 * Arguments:
 *	Already op_zydecode_arg.c has saved all the inputs in zydecode_args and dglvnp.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	zydecode_args will be reset to 0 after each invocation.
 *
 * Notes:
 * ------------------------------------------------------------------------------------------------------------
 */
#include "mdef.h"

#include "mv_stent.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zyencode_zydecode_def.h"	/* for ARG1_LCL, ARG1_GBL, ARG2_LCL, and ARG2_GBL */
#include "op_zyencode_zydecode.h"	/* for zydecode_glvn_ptr */
#include "gvsub2str.h"
#include "mvalconv.h"
#include "deferred_events_queue.h"
#include "sgnl.h"

GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF int			zydecode_args;
GBLREF zydecode_glvn_ptr	dglvnp;
GBLREF mv_stent			*mv_chain;
GBLREF bool			undef_inhibit;
GBLREF volatile int4		outofband;
#ifdef DEBUG
GBLREF lv_val			*active_lv;
#endif

void op_zydecode(void)
{
	boolean_t		found, is_base_var;
	gvname_info		*gblp1, *gblp2;
	int			key_end, dollardata_src, status, cnt_fmt = 0, cnt = 0, len, size = MAX_ZWR_KEY_SZ, err;
	size_t			json_size = 0, realloc_size = 0;
	lv_val			*dst_lv, *lv, *base_lv, *src_lv;
	lvTree			*lvt;
	lvTreeNode		*node, *nodep[MAX_LVSUBSCRIPTS];
	lvname_info		lvn_info;
	mstr			opstr;
	mval			*value, *subsc, tmp_mval, temp_mv, subsc_arr[MAX_LVSUBSCRIPTS];
				/* Add 2 to name_buff for NUL byte and '^' for globals */
	unsigned char		buff[MAX_ZWR_KEY_SZ], err_buff[MAX_ZWR_KEY_SZ], name_buff[YDB_MAX_IDENT + 2],
				*ptr, *ptr2, *ptr_top, *end_buff;
	char			*format = "JSON", *json = NULL, *tjson, error_str[YDB_MAX_ERRORMSG];
	bool			undef_inhibit_save = false;
	ydb_buffer_t		variable, subscripts[YDB_MAX_SUBS];
#	ifdef DEBUG
	lv_val			*orig_active_lv;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_STRLEN >= MAX_ZWR_KEY_SZ);
	assert ((zydecode_args == (ARG1_LCL | ARG2_LCL)) ||
		(zydecode_args == (ARG1_LCL | ARG2_GBL)) ||
		(zydecode_args == (ARG1_GBL | ARG2_LCL)) ||
		(zydecode_args == (ARG1_GBL | ARG2_GBL)));
	/* Need to protect value from stpgcol */
	PUSH_MV_STENT(MVST_MVAL);
	value = &mv_chain->mv_st_cont.mvs_mval;
	value->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before value gets initialized below */
	/* Need to protect subsc from stpgcol */
	PUSH_MV_STENT(MVST_MVAL);
	subsc = &mv_chain->mv_st_cont.mvs_mval;
	subsc->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before subsc gets initialized below */
	gblp1 = dglvnp->gblp[IND1];
	gblp2 = dglvnp->gblp[IND2];
	DEBUG_ONLY(orig_active_lv = active_lv;)
	if (ARG2_IS_GBL(zydecode_args))
	{
		gvname_env_restore(gblp2);
		/* now $DATA will be done for gvn2. op_gvdata input parameters are set in the form of some GBLREF */
		op_gvdata(value);
		dollardata_src = MV_FORCE_INTD(value);
		if ((0 == dollardata_src) || (10 == dollardata_src))
		{	/* NOOP - zydecode with empty unsubscripted source local variable */
			assert(orig_active_lv == active_lv);
			UNDO_ACTIVE_LV(actlv_op_zydecode1); /* kill "dst" and parents as applicable if $data(dst)=0 */
			if (ARG1_IS_GBL(zydecode_args))
				gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
			POP_MV_STENT();	/* subsc */
			POP_MV_STENT();	/* value */
			zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);	/* no JSON chunk count in source global */
		}
		if (ARG1_IS_GBL(zydecode_args))
		{	/*==================== ZYDECODE ^gvn1=^gvn2 =====================*/
			zyencode_zydecode_desc_check(ERR_ZYDECODEDESC); /* will not proceed if one is descendant of another */
			op_gvget(value);
			/* Note assignment - there must be at least one JSON node */
			if ((0 == value->str.len) || (1 > (cnt = MV_FORCE_INTD(value))))
			{
				gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);
			}
			subsc->str.addr = (char *)buff;
			key_end = gv_currkey->end;
			for (int i = 1; i <= cnt; i++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", i);
				gv_currkey->end = key_end;
				mval2subsc(subsc, gv_currkey, gv_cur_region->std_null_coll);
				undef_inhibit_save = undef_inhibit;
				undef_inhibit = true;	/* handle undefined errors here so we can reset zydecode_args */
				found = op_gvget(value);
				undef_inhibit = undef_inhibit_save;
				if (found && value->str.len)
				{
					if ((json_size + value->str.len + 1) > realloc_size)
					{
						/* 1 for the NUL, double it to reduce the number of potential allocations */
						realloc_size = (json_size + value->str.len) * 2 + 1;
						tjson = realloc(json, realloc_size);
						if (NULL == tjson)
						{
							/* free() is not used because of realloc() use which
							 * gtm_malloc() does not support
							 */
							system_free(json);
							err = errno;
							/* store destination as naked indicator in gv_currkey */
							gvname_env_restore(gblp1);
							POP_MV_STENT();	/* subsc */
							POP_MV_STENT();	/* value */
							zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
							SNPRINTF(error_str, YDB_MAX_ERRORMSG,
								"realloc(): %s while allocating %u bytes",
								STRERROR(err), realloc_size);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZYDECODEINCOMPL, 0,
								ERR_SYSCALL, 5, RTS_ERROR_STRING(error_str), CALLFROM, err);
						}
						json = tjson;
					}
					memcpy(&json[json_size], value->str.addr, value->str.len);
					json_size += value->str.len;
				} else
				{
					POP_MV_STENT();	/* subsc */
					POP_MV_STENT();	/* value */
					zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
					sgnl_gvundef();	/* Issue GVUNDEF error */
				}
			}
			json[json_size] = '\0';
			gvname_env_restore(gblp1);
			name_buff[0] = '^';	/* make sure ydb_decode_s() knows it's a global */
			/* gv_target->gvname.var_name.len should never be bigger than YDB_MAX_IDENT, but we'll be extra safe */
			len = (gv_target->gvname.var_name.len < YDB_MAX_IDENT) ? gv_target->gvname.var_name.len : YDB_MAX_IDENT;
			end_buff = (unsigned char *)stpncpy((char *)&name_buff[1], gv_target->gvname.var_name.addr, len);
			*end_buff = '\0';	/* make sure it's NUL-terminated for YDB_STRING_TO_BUFFER */
			YDB_STRING_TO_BUFFER((char *)name_buff, &variable);
			cnt = 0;
			ptr = (unsigned char *)&gv_currkey->base[variable.len_used];
			ptr_top = (unsigned char *)gv_currkey->base + gv_currkey->end;
			for ( ; ptr < ptr_top; )
			{
				opstr.addr = (char *)buff;
				opstr.len = MAX_ZWR_KEY_SZ;
				ptr2 = gvsub2str(ptr, &opstr, FALSE);
				tmp_mval.mvtype = MV_STR;
				tmp_mval.str.addr = (char *)buff;
				tmp_mval.str.len = INTCAST(ptr2 - buff);
				YDB_MALLOC_BUFFER(&subscripts[cnt], tmp_mval.str.len);
				subscripts[cnt].len_used = tmp_mval.str.len;
				memcpy(subscripts[cnt].buf_addr, tmp_mval.str.addr, tmp_mval.str.len);
				cnt++;
				/* we have now appended the correctly transformed subscript */
				while (*ptr++)
					;	/* skip to start of next subscript */
			}
			assert(ptr == ptr_top);
			status = ydb_decode_s(&variable, cnt, subscripts, format, json);
			/* free() is not used because of realloc() use which gtm_malloc() does not support */
			system_free(json);
			for (int i = 0; i < cnt; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			gvname_env_restore(gblp1);	/* naked indicator is restored into gv_currkey */
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYDECODEINCOMPL);
			}
		} else
		{	/*==================== ZYDECODE lvn1=^gvn2 =====================*/
			assert(ARG1_IS_LCL(zydecode_args));
			assert(dglvnp->lclp[IND1]);
			lv = dglvnp->lclp[IND1];
			is_base_var = LV_IS_BASE_VAR(lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
			while (lv != base_lv)
			{
				assert(!LV_IS_BASE_VAR(lv));
				nodep[cnt_fmt++] = (lvTreeNode *)lv;
				lvt = LV_GET_PARENT_TREE(lv);
				assert(NULL != lvt);
				assert(lvt->base_lv == base_lv);
				lv = (lv_val *)LVT_PARENT(lvt);
				assert(NULL != lv);
			}
			end_buff = format_lvname(base_lv, name_buff, size);
			/* the variable was indirect, and was not found in the frame's l_symtab, so scan curr_symval to find it */
			if (name_buff == end_buff)
				FIND_INDIRECT_SYMBOL_NAME(base_lv, name_buff, end_buff);
			*end_buff = '\0';	/* make sure it's NUL-terminated for YDB_STRING_TO_BUFFER */
			YDB_STRING_TO_BUFFER((char *)name_buff, &variable);
			for (cnt = cnt_fmt - 1; cnt >= 0; cnt--)
			{
				node = nodep[cnt];
				/* Get node key into "temp_mv" depending on the structure type of "node" */
				LV_NODE_GET_KEY(node, &temp_mv);
				MV_FORCE_STRD(&temp_mv);
				YDB_MALLOC_BUFFER(&subscripts[cnt_fmt - 1 - cnt], temp_mv.str.len);
				subscripts[cnt_fmt - 1 - cnt].len_used = temp_mv.str.len;
				memcpy(subscripts[cnt_fmt - 1 - cnt].buf_addr, temp_mv.str.addr, temp_mv.str.len);
			}
			op_gvget(value);
			/* Note assignment - there must be at least one JSON node */
			if ((0 == value->str.len) || (1 > (cnt = MV_FORCE_INTD(value))))
			{
				UNDO_ACTIVE_LV(actlv_op_zydecode2); /* kill "dst" and parents as applicable if $data(dst)=0 */
				gvname_env_restore(gblp2);	/* store source as naked indicator in gv_currkey */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);
			}
			subsc->str.addr = (char *)buff;
			key_end = gv_currkey->end;
			for (int i = 1; i <= cnt; i++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", i);
				gv_currkey->end = key_end;
				mval2subsc(subsc, gv_currkey, gv_cur_region->std_null_coll);
				undef_inhibit_save = undef_inhibit;
				undef_inhibit = true;	/* handle undefined errors here so we can reset zydecode_args */
				found = op_gvget(value);
				undef_inhibit = undef_inhibit_save;
				if (found && value->str.len)
				{
					if ((json_size + value->str.len + 1) > realloc_size)
					{
						/* 1 for the NUL, double it to reduce the number of potential allocations */
						realloc_size = (json_size + value->str.len) * 2 + 1;
						tjson = realloc(json, realloc_size);
						if (NULL == tjson)
						{
							/* free() is not used because of realloc() use which
							 * gtm_malloc() does not support
							 */
							system_free(json);
							err = errno;
							/* kill "dst" and parents as applicable if $data(dst)=0 */
							UNDO_ACTIVE_LV(actlv_op_zydecode3);
							/* store source as naked indicator in gv_currkey */
							gvname_env_restore(gblp2);
							POP_MV_STENT();	/* subsc */
							POP_MV_STENT();	/* value */
							zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
							SNPRINTF(error_str, YDB_MAX_ERRORMSG,
								"realloc(): %s while allocating %u bytes",
								STRERROR(err), realloc_size);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZYDECODEINCOMPL, 0,
								ERR_SYSCALL, 5, RTS_ERROR_STRING(error_str), CALLFROM, err);
						}
						json = tjson;
					}
					memcpy(&json[json_size], value->str.addr, value->str.len);
					json_size += value->str.len;
				} else
				{
					/* kill "dst" and parents as applicable if $data(dst)=0 */
					UNDO_ACTIVE_LV(actlv_op_zydecode3);
					POP_MV_STENT();	/* subsc */
					POP_MV_STENT();	/* value */
					zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
					sgnl_gvundef();	/* Issue GVUNDEF error */
				}
			}
			json[json_size] = '\0';
			status = ydb_decode_s(&variable, cnt_fmt, subscripts, format, json);
			/* free() is not used because of realloc() use which gtm_malloc() does not support */
			system_free(json);
			for (int i = 0; i < cnt_fmt; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			gvname_env_restore(gblp2);	/* naked indicator is restored into gv_currkey */
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)	/* handle the error here so the previous cleanup doesn't need to be duplicated */
			{
				UNDO_ACTIVE_LV(actlv_op_zydecode3); /* kill "dst" and parents as applicable if $data(dst)=0 */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYDECODEINCOMPL);
			}
		}
	} else
	{	/* source is local */
		src_lv = dglvnp->lclp[IND2];
		op_fndata(src_lv, &tmp_mval);
		dollardata_src = MV_FORCE_INTD(&tmp_mval);
		if ((0 == dollardata_src) || (10 == dollardata_src))
		{	/* NOOP - zydecode with empty unsubscripted source local variable */
			assert(orig_active_lv == active_lv);
			UNDO_ACTIVE_LV(actlv_op_zydecode1); /* kill "dst" and parents as applicable if $data(dst)=0 */
			POP_MV_STENT();	/* subsc */
			POP_MV_STENT();	/* value */
			zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);	/* no JSON chunk count in source global */
		}
		if (ARG1_IS_LCL(zydecode_args))
		{	/*==================== ZYDECODE lvn1=lvn2 =====================*/
			zyencode_zydecode_desc_check(ERR_ZYDECODEDESC); /* will not proceed if one is descendant of another */
			assert(dglvnp->lclp[IND1]);
			lv = dglvnp->lclp[IND1];
			is_base_var = LV_IS_BASE_VAR(lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
			while (lv != base_lv)
			{
				assert(!LV_IS_BASE_VAR(lv));
				nodep[cnt_fmt++] = (lvTreeNode *)lv;
				lvt = LV_GET_PARENT_TREE(lv);
				assert(NULL != lvt);
				assert(lvt->base_lv == base_lv);
				lv = (lv_val *)LVT_PARENT(lvt);
				assert(NULL != lv);
			}
			end_buff = format_lvname(base_lv, name_buff, size);
			/* the variable was indirect, and was not found in the frame's l_symtab, so scan curr_symval to find it */
			if (name_buff == end_buff)
				FIND_INDIRECT_SYMBOL_NAME(base_lv, name_buff, end_buff);
			*end_buff = '\0';	/* make sure it's NUL-terminated for YDB_STRING_TO_BUFFER */
			YDB_STRING_TO_BUFFER((char *)name_buff, &variable);
			for (cnt = cnt_fmt - 1; cnt >= 0; cnt--)
			{
				node = nodep[cnt];
				/* Get node key into "temp_mv" depending on the structure type of "node" */
				LV_NODE_GET_KEY(node, &temp_mv);
				MV_FORCE_STRD(&temp_mv);
				YDB_MALLOC_BUFFER(&subscripts[cnt_fmt - 1 - cnt], temp_mv.str.len);
				subscripts[cnt_fmt - 1 - cnt].len_used = temp_mv.str.len;
				memcpy(subscripts[cnt_fmt - 1 - cnt].buf_addr, temp_mv.str.addr, temp_mv.str.len);
			}
			assert(dglvnp->lclp[IND2]);
			lv = dglvnp->lclp[IND2];
			is_base_var = LV_IS_BASE_VAR(lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
			/* Note assignment - there must be at least one JSON node */
			if ((0 == lv->v.str.len) || (1 > (cnt = MV_FORCE_INTD(&lv->v))))
			{
				UNDO_ACTIVE_LV(actlv_op_zydecode2); /* kill "dst" and parents as applicable if $data(dst)=0 */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);
			}
			subsc->str.addr = (char *)buff;
			for (int i = 1; i <= cnt; i++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", i);
				undef_inhibit_save = undef_inhibit;
				undef_inhibit = true;	/* handle undefined errors here so we can reset zydecode_args */
				dst_lv = op_getindx(VARLSTCNT(2) lv, subsc);
				undef_inhibit = undef_inhibit_save;
				if (dst_lv->v.str.len)
				{
					if ((json_size + dst_lv->v.str.len + 1) > realloc_size)
					{
						/* 1 for the NUL, double it to reduce the number of potential allocations */
						realloc_size = (json_size + dst_lv->v.str.len) * 2 + 1;
						tjson = realloc(json, realloc_size);
						if (NULL == tjson)
						{
							/* free() is not used because of realloc() use which
							 * gtm_malloc() does not support
							 */
							system_free(json);
							err = errno;
							/* kill "dst" and parents as applicable if $data(dst)=0 */
							UNDO_ACTIVE_LV(actlv_op_zydecode3);
							POP_MV_STENT();	/* subsc */
							POP_MV_STENT();	/* value */
							zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
							SNPRINTF(error_str, YDB_MAX_ERRORMSG,
								"realloc(): %s while allocating %u bytes",
								STRERROR(err), realloc_size);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZYDECODEINCOMPL, 0,
								ERR_SYSCALL, 5, RTS_ERROR_STRING(error_str), CALLFROM, err);
						}
						json = tjson;
					}
					memcpy(&json[json_size], dst_lv->v.str.addr, dst_lv->v.str.len);
					json_size += dst_lv->v.str.len;
				} else
				{
					BUILD_FORMAT_KEY_MVALS(lv, subsc_arr, &lvn_info);
					lvn_info.lv_subs[lvn_info.total_lv_subs - 1] = subsc;
					MV_FORCE_STR(lvn_info.lv_subs[lvn_info.total_lv_subs - 1]);
					lvn_info.total_lv_subs++;
					end_buff = format_key_mvals(err_buff, SIZEOF(err_buff), &lvn_info);
					/* kill "dst" and parents as applicable if $data(dst)=0 */
					UNDO_ACTIVE_LV(actlv_op_zydecode3);
					POP_MV_STENT();	/* subsc */
					POP_MV_STENT();	/* value */
					zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
					RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(4) ERR_LVUNDEF, 2, end_buff - err_buff, err_buff);
				}
			}
			json[json_size] = '\0';
			status = ydb_decode_s(&variable, cnt_fmt, subscripts, format, json);
			/* free() is not used because of realloc() use which gtm_malloc() does not support */
			system_free(json);
			for (int i = 0; i < cnt_fmt; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)	/* handle the error here so the previous cleanup doesn't need to be duplicated */
			{
				UNDO_ACTIVE_LV(actlv_op_zydecode3); /* kill "dst" and parents as applicable if $data(dst)=0 */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYDECODEINCOMPL);
			}
		} else
		{	/*==================== ZYDECODE ^gvn1=lvn2 =====================*/
			assert(ARG1_IS_GBL(zydecode_args) && ARG2_IS_LCL(zydecode_args));
			assert(dglvnp->lclp[IND2]);
			lv = dglvnp->lclp[IND2];
			is_base_var = LV_IS_BASE_VAR(lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(lv) : lv;
			/* Note assignment - there must be at least one JSON node */
			if ((0 == lv->v.str.len) || (1 > (cnt = MV_FORCE_INTD(&lv->v))))
			{
				gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYDECODEWRONGCNT);
			}
			subsc->str.addr = (char *)buff;
			for (int i = 1; i <= cnt; i++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", i);
				undef_inhibit_save = undef_inhibit;
				undef_inhibit = true;	/* handle undefined errors here so we can reset zydecode_args */
				dst_lv = op_getindx(VARLSTCNT(2) lv, subsc);
				undef_inhibit = undef_inhibit_save;
				if (dst_lv->v.str.len)
				{
					if ((json_size + dst_lv->v.str.len + 1) > realloc_size)
					{
						/* 1 for the NUL, double it to reduce the number of potential allocations */
						realloc_size = (json_size + dst_lv->v.str.len) * 2 + 1;
						tjson = realloc(json, realloc_size);
						if (NULL == tjson)
						{
							/* free() is not used because of realloc() use which
							 * gtm_malloc() does not support
							 */
							system_free(json);
							err = errno;
							/* store destination as naked indicator in gv_currkey */
							gvname_env_restore(gblp1);
							SNPRINTF(error_str, YDB_MAX_ERRORMSG,
								"realloc(): %s while allocating %u bytes",
								STRERROR(err), realloc_size);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZYDECODEINCOMPL, 0,
								ERR_SYSCALL, 5, RTS_ERROR_STRING(error_str), CALLFROM, err);
						}
						json = tjson;
					}
					memcpy(&json[json_size], dst_lv->v.str.addr, dst_lv->v.str.len);
					json_size += dst_lv->v.str.len;
				} else
				{
					BUILD_FORMAT_KEY_MVALS(lv, subsc_arr, &lvn_info);
					lvn_info.lv_subs[lvn_info.total_lv_subs - 1] = subsc;
					MV_FORCE_STR(lvn_info.lv_subs[lvn_info.total_lv_subs - 1]);
					lvn_info.total_lv_subs++;
					end_buff = format_key_mvals(err_buff, SIZEOF(err_buff), &lvn_info);
					gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
					POP_MV_STENT();	/* subsc */
					POP_MV_STENT();	/* value */
					zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
					RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(4) ERR_LVUNDEF, 2, end_buff - err_buff, err_buff);
				}
			}
			json[json_size] = '\0';
			gvname_env_restore(gblp1);
			name_buff[0] = '^';	/* make sure ydb_decode_s() knows it's a global */
			/* gv_target->gvname.var_name.len should never be bigger than YDB_MAX_IDENT, but we'll be extra safe */
			len = (gv_target->gvname.var_name.len < YDB_MAX_IDENT) ? gv_target->gvname.var_name.len : YDB_MAX_IDENT;
			end_buff = (unsigned char *)stpncpy((char *)&name_buff[1], gv_target->gvname.var_name.addr, len);
			*end_buff = '\0';	/* make sure it's NUL-terminated for YDB_STRING_TO_BUFFER */
			YDB_STRING_TO_BUFFER((char *)name_buff, &variable);
			cnt = 0;
			ptr = (unsigned char *)&gv_currkey->base[variable.len_used];
			ptr_top = (unsigned char *)gv_currkey->base + gv_currkey->end;
			for ( ; ptr < ptr_top; )
			{
				opstr.addr = (char *)buff;
				opstr.len = MAX_ZWR_KEY_SZ;
				ptr2 = gvsub2str(ptr, &opstr, FALSE);
				tmp_mval.mvtype = MV_STR;
				tmp_mval.str.addr = (char *)buff;
				tmp_mval.str.len = INTCAST(ptr2 - buff);
				YDB_MALLOC_BUFFER(&subscripts[cnt], tmp_mval.str.len);
				subscripts[cnt].len_used = tmp_mval.str.len;
				memcpy(subscripts[cnt].buf_addr, tmp_mval.str.addr, tmp_mval.str.len);
				cnt++;
				/* we have now appended the correctly transformed subscript */
				while (*ptr++)
					;	/* skip to start of next subscript */
			}
			assert(ptr == ptr_top);
			status = ydb_decode_s(&variable, cnt, subscripts, format, json);
			/* free() is not used because of realloc() use which gtm_malloc() does not support */
			system_free(json);
			for (int i = 0; i < cnt; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYDECODEINCOMPL);
			}
		}
	}
	POP_MV_STENT();	/* subsc */
	POP_MV_STENT();	/* value */
	zydecode_args = 0;	/* Must reset to zero to reuse zydecode */
}
