/****************************************************************
 *								*
 * Copyright (c) 2025-2026 YottaDB LLC and/or its subsidiaries.	*
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
 * op_zyencode.c
 * ==============
 * Description:
 *	Main routine of ZYENCODE command.
 *
 * Arguments:
 *	Already op_zyencode_arg.c has saved all the inputs in zyencode_args and eglvnp.
 *
 * Return:
 *      none
 *
 * Side Effects:
 *	zyencode_args will be reset to 0 after each invocation.
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
#include "op_zyencode_zydecode.h"	/* for zyencode_glvn_ptr */
#include "gvsub2str.h"
#include "mvalconv.h"
#include "stringpool.h"
#include "deferred_events_queue.h"

GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF int			zyencode_args;
GBLREF zyencode_glvn_ptr	eglvnp;
GBLREF mv_stent			*mv_chain;
GBLREF ydb_string_t		zyencode_ret;
GBLREF volatile int4		outofband;
#ifdef DEBUG
GBLREF lv_val			*active_lv;
#endif

void op_zyencode(void)
{
	boolean_t		is_base_var;
	gvname_info		*gblp1, *gblp2;
	int			key_end, dollardata_src, status, cnt_fmt = 0, cnt = 0, cnt_save, size = MAX_ZWR_KEY_SZ, rec_size;
	unsigned long		length;
	unsigned long long	len;
	lv_val			*dst_lv, *lv, *base_lv, *orig_lv, *src_lv;
	lvTree			*lvt;
	lvTreeNode		*node, *nodep[MAX_LVSUBSCRIPTS];
	mstr			opstr;
	mval			*value, *subsc, tmp_mval, temp_mv;
				/* Add 1 to name_buff for the '^' for globals */
	unsigned char		buff[MAX_ZWR_KEY_SZ], gv_buff[YDB_MAX_IDENT + 1], *ptr, *ptr2, *ptr_top, *end_buff;
	char			*format = "JSON", *address;
	ydb_buffer_t		variable, subscripts[YDB_MAX_SUBS];
#	ifdef DEBUG
	lv_val			*orig_active_lv;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_STRLEN >= MAX_ZWR_KEY_SZ);
	assert ((zyencode_args == (ARG1_LCL | ARG2_LCL)) ||
		(zyencode_args == (ARG1_LCL | ARG2_GBL)) ||
		(zyencode_args == (ARG1_GBL | ARG2_LCL)) ||
		(zyencode_args == (ARG1_GBL | ARG2_GBL)));
	/* Need to protect value from stpgcol */
	PUSH_MV_STENT(MVST_MVAL);
	value = &mv_chain->mv_st_cont.mvs_mval;
	value->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before value gets initialized below */
	gblp1 = eglvnp->gblp[IND1];
	gblp2 = eglvnp->gblp[IND2];
	DEBUG_ONLY(orig_active_lv = active_lv;)
	if (ARG2_IS_GBL(zyencode_args))
	{
		gvname_env_restore(gblp2);
		/* now $DATA will be done for gvn2. op_gvdata input parameters are set in the form of some GBLREF */
		op_gvdata(value);
		dollardata_src = MV_FORCE_INTD(value);
		if (0 == dollardata_src)
		{	/* NOOP - zyencode with empty unsubscripted source local variable */
			assert(orig_active_lv == active_lv);
			UNDO_ACTIVE_LV(actlv_op_zyencode1); /* kill "dst" and parents as applicable if $data(dst)=0 */
			if (ARG1_IS_GBL(zyencode_args))
				gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
			POP_MV_STENT();	/* value */
			zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYENCODESRCUNDEF);
		}
		if (ARG1_IS_GBL(zyencode_args))
		{	/*==================== ZYENCODE ^gvn1=^gvn2 =====================*/
			zyencode_zydecode_desc_check(ERR_ZYENCODEDESC); /* will not proceed if one is descendant of another */
			variable.buf_addr = (char *)gv_buff;
			variable.len_alloc = YDB_MAX_IDENT + 1;	/* 1 for the ^ */
			variable.buf_addr[0] = '^';	/* make sure ydb_encode_s() knows it's a global */
			/* gv_target->gvname.var_name.len should never be bigger than YDB_MAX_IDENT, but we'll be extra safe */
			len = (gv_target->gvname.var_name.len < YDB_MAX_IDENT) ? gv_target->gvname.var_name.len : YDB_MAX_IDENT;
			memcpy(&variable.buf_addr[1], gv_target->gvname.var_name.addr, len);
			variable.len_used = gv_target->gvname.var_name.len + 1;	/* 1 for the ^ */
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
				memcpy(subscripts[cnt++].buf_addr, tmp_mval.str.addr, tmp_mval.str.len);
				/* we have now copied the correctly transformed subscript for ydb_encode_s() */
				while (*ptr++)
					;	/* skip to start of next subscript */
			}
			cnt_save = cnt;
			status = ydb_encode_s(&variable, cnt, subscripts, format, &zyencode_ret);
			gvname_env_restore(gblp1);
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				for (int i = 0; i < cnt_save; i++)
					YDB_FREE_BUFFER(&subscripts[i]);
				POP_MV_STENT();	/* value */
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYENCODEINCOMPL);
			}
			key_end = gv_currkey->end;
			address = zyencode_ret.address;
			length = zyencode_ret.length;
			rec_size = gblp1->s_gv_cur_region->max_rec_size;
			cnt = 1;
			for (unsigned long remain = length; 0 != remain; remain -= value->str.len, cnt++)
			{
				tmp_mval.mvtype = MV_STR;
				tmp_mval.str.addr = (char *)buff;
				tmp_mval.str.len = SNPRINTF(tmp_mval.str.addr, MAX_NUM_SIZE, "%d", cnt);
				mval2subsc(&tmp_mval, gv_currkey, gv_cur_region->std_null_coll);
				value->mvtype = MV_STR;
				value->str.len = (rec_size < length) ? rec_size : length;
				value->str.addr = address;
				op_gvput(value);
				gv_currkey->end = key_end;
				length -= value->str.len;
				address += value->str.len;
			}
			gv_currkey->base[key_end] = KEY_DELIMITER;
			value->str.addr = (char *)buff;
			value->str.len = SNPRINTF(value->str.addr, MAX_NUM_SIZE, "%d", --cnt);
			op_gvput(value);
			gvname_env_restore(gblp1);	/* naked indicator is restored into gv_currkey */
			/* zyencode_ret.address was returned by Jansson in ydb_encode_s(), which used the system malloc() */
			system_free(zyencode_ret.address);	/* free after we store the chunk number in op_gvput */
			zyencode_ret.address = NULL;	/* Need to set NULL because this handler can be called deep in the stack,
							 * and there  is no other way to ensure no double free() can happen,
							 * other than always setting it NULL after it is free()'d.
							 */
			zyencode_ret.length = 0;
			for (int i = 0; i < cnt_save; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
		} else
		{	/*==================== ZYENCODE lvn1=^gvn2 =====================*/
			assert(ARG1_IS_LCL(zyencode_args));
			/* At this time gv_currkey already points to gblp2 */
			/* Need to protect subsc created from global variable subscripts from stpgcol */
			PUSH_MV_STENT(MVST_MVAL);
			subsc = &mv_chain->mv_st_cont.mvs_mval;
			subsc->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before it is set below */
			variable.buf_addr = (char *)gv_buff;
			variable.len_alloc = YDB_MAX_IDENT + 1;	/* 1 for the ^ */
			variable.buf_addr[0] = '^';	/* make sure ydb_encode_s() knows it's a global */
			/* gv_target->gvname.var_name.len should never be bigger than YDB_MAX_IDENT, but we'll be extra safe */
			len = (gv_target->gvname.var_name.len < YDB_MAX_IDENT) ? gv_target->gvname.var_name.len : YDB_MAX_IDENT;
			memcpy(&variable.buf_addr[1], gv_target->gvname.var_name.addr, len);
			variable.len_used = gv_target->gvname.var_name.len + 1;	/* 1 for the ^ */
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
				memcpy(subscripts[cnt++].buf_addr, tmp_mval.str.addr, tmp_mval.str.len);
				/* we have now copied the correctly transformed subscript for ydb_encode_s() */
				while (*ptr++)
					;	/* skip to start of next subscript */
			}
			cnt_save = cnt;
			status = ydb_encode_s(&variable, cnt, subscripts, format, &zyencode_ret);
			gvname_env_restore(gblp2);	/* naked indicator is restored into gv_currkey */
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				for (int i = 0; i < cnt_save; i++)
					YDB_FREE_BUFFER(&subscripts[i]);
				UNDO_ACTIVE_LV(actlv_op_zyencode2); /* kill "dst" and parents as applicable if $data(dst)=0 */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYENCODEINCOMPL);
			}
			assert(eglvnp->lclp[IND1]);
			dst_lv = eglvnp->lclp[IND1];
			orig_lv = dst_lv;
			is_base_var = LV_IS_BASE_VAR(dst_lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(dst_lv) : dst_lv;
			address = zyencode_ret.address;
			length = zyencode_ret.length;
			cnt = 1;
			for (unsigned long remain = length; 0 != remain; remain -= value->str.len, cnt++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.addr = (char *)buff;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", cnt);
				s2pool(&subsc->str);
				dst_lv = op_putindx(VARLSTCNT(2) orig_lv, subsc);
				value->mvtype = MV_STR;
				value->str.len = (YDB_MAX_STR < length) ? YDB_MAX_STR : length;
				value->str.addr = address;
				dst_lv->v = *value;
				s2pool(&dst_lv->v.str);
				length -= value->str.len;
				address += value->str.len;
			}
			/* zyencode_ret.address was returned by Jansson in ydb_encode_s(), which used the system malloc() */
			system_free(zyencode_ret.address);
			zyencode_ret.address = NULL;	/* Need to set NULL because this handler can be called deep in the stack,
							 * and there  is no other way to ensure no double free() can happen,
							 * other than always setting it NULL after it is free()'d.
							 */
			zyencode_ret.length = 0;
			for (int i = 0; i < cnt_save; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE);
			orig_lv->v.str.addr = (char *)stringpool.free;
			orig_lv->v.str.len = SNPRINTF(orig_lv->v.str.addr, MAX_NUM_SIZE, "%d", --cnt);
			stringpool.free += orig_lv->v.str.len;
			assert(stringpool.free >= stringpool.base);
			assert(stringpool.free <= stringpool.top);
			orig_lv->v.mvtype = MV_STR;
			MV_FORCE_NUMD(&orig_lv->v);
			gvname_env_restore(gblp2);	/* naked indicator is restored into gv_currkey */
			POP_MV_STENT();     /* subsc */
		}
	} else
	{	/* source is local */
		src_lv = eglvnp->lclp[IND2];
		op_fndata(src_lv, &tmp_mval);
		dollardata_src = MV_FORCE_INTD(&tmp_mval);
		if (0 == dollardata_src)
		{	/* NOOP - zyencode with empty unsubscripted source local variable */
			assert(orig_active_lv == active_lv);
			UNDO_ACTIVE_LV(actlv_op_zyencode1); /* kill "dst" and parents as applicable if $data(dst)=0 */
			POP_MV_STENT();	/* value */
			zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYENCODESRCUNDEF);
		}
		if (ARG1_IS_LCL(zyencode_args))
		{	/*==================== ZYENCODE lvn1=lvn2 =====================*/
			zyencode_zydecode_desc_check(ERR_ZYENCODEDESC); /* will not proceed if one is descendant of another */
			PUSH_MV_STENT(MVST_MVAL);
			subsc = &mv_chain->mv_st_cont.mvs_mval;
			subsc->mvtype = 0; /* initialize mval in the M-stack in case stp_gcol gets called before it is set below */
			assert(eglvnp->lclp[IND2]);
			lv = eglvnp->lclp[IND2];
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
			end_buff = format_lvname(base_lv, buff, size);
			/* the variable was indirect, and was not found in the frame's l_symtab, so scan curr_symval to find it */
			if (buff == end_buff)
				FIND_INDIRECT_SYMBOL_NAME(base_lv, buff, end_buff);
			*end_buff = '\0';
			YDB_STRING_TO_BUFFER((char *)buff, &variable);
			for (cnt = cnt_fmt - 1; cnt >= 0; cnt--)
			{
				node = nodep[cnt];
				/* Get node key into "temp_mv" depending on the structure type of "node" */
				LV_NODE_GET_KEY(node, &temp_mv);
				MV_FORCE_STRD(&temp_mv);
				YDB_MALLOC_BUFFER(&subscripts[cnt_fmt - 1 - cnt], temp_mv.str.len);
				subscripts[cnt_fmt - 1 - cnt].len_used = temp_mv.str.len;
				memcpy(subscripts[cnt_fmt - 1 - cnt].buf_addr, temp_mv.str.addr, temp_mv.str.len);
				/* we have now copied the subscript for ydb_encode_s() */
			}
			cnt_save = cnt_fmt;
			status = ydb_encode_s(&variable, cnt_fmt, subscripts, format, &zyencode_ret);
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				for (int i = 0; i < cnt_save; i++)
					YDB_FREE_BUFFER(&subscripts[i]);
				UNDO_ACTIVE_LV(actlv_op_zyencode2); /* kill "dst" and parents as applicable if $data(dst)=0 */
				POP_MV_STENT();	/* subsc */
				POP_MV_STENT();	/* value */
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYENCODEINCOMPL);
			}
			assert(eglvnp->lclp[IND1]);
			dst_lv = eglvnp->lclp[IND1];
			orig_lv = dst_lv;
			is_base_var = LV_IS_BASE_VAR(dst_lv);
			base_lv = !is_base_var ? LV_GET_BASE_VAR(dst_lv) : dst_lv;
			address = zyencode_ret.address;
			length = zyencode_ret.length;
			cnt = 1;
			for (unsigned long remain = length; 0 != remain; remain -= value->str.len, cnt++)
			{
				subsc->mvtype = MV_STR;
				subsc->str.addr = (char *)buff;
				subsc->str.len = SNPRINTF(subsc->str.addr, MAX_NUM_SIZE, "%d", cnt);
				s2pool(&subsc->str);
				dst_lv = op_putindx(VARLSTCNT(2) orig_lv, subsc);
				value->mvtype = MV_STR;
				value->str.len = (YDB_MAX_STR < length) ? YDB_MAX_STR : length;
				value->str.addr = address;
				dst_lv->v = *value;
				s2pool(&dst_lv->v.str);
				length -= value->str.len;
				address += value->str.len;
			}
			/* zyencode_ret.address was returned by Jansson in ydb_encode_s(), which used the system malloc() */
			system_free(zyencode_ret.address);
			zyencode_ret.address = NULL;	/* Need to set NULL because this handler can be called deep in the stack,
							 * and there  is no other way to ensure no double free() can happen,
							 * other than always setting it NULL after it is free()'d.
							 */
			zyencode_ret.length = 0;
			for (int i = 0; i < cnt_save; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
			ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE);
			orig_lv->v.str.addr = (char *)stringpool.free;
			orig_lv->v.str.len = SNPRINTF(orig_lv->v.str.addr, MAX_NUM_SIZE, "%d", --cnt);
			stringpool.free += orig_lv->v.str.len;
			assert(stringpool.free >= stringpool.base);
			assert(stringpool.free <= stringpool.top);
			orig_lv->v.mvtype = MV_STR;
			MV_FORCE_NUMD(&orig_lv->v);
			POP_MV_STENT();     /* subsc */
		} else
		{	/*==================== ZYENCODE ^gvn1=lvn2 =====================*/
			assert(ARG1_IS_GBL(zyencode_args) && ARG2_IS_LCL(zyencode_args));
			assert(eglvnp->lclp[IND2]);
			lv = eglvnp->lclp[IND2];
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
			end_buff = format_lvname(base_lv, buff, size);
			/* the variable was indirect, and was not found in the frame's l_symtab, so scan curr_symval to find it */
			if (buff == end_buff)
				FIND_INDIRECT_SYMBOL_NAME(base_lv, buff, end_buff);
			*end_buff = '\0';
			YDB_STRING_TO_BUFFER((char *)buff, &variable);
			for (cnt = cnt_fmt - 1; cnt >= 0; cnt--)
			{
				node = nodep[cnt];
				/* Get node key into "temp_mv" depending on the structure type of "node" */
				LV_NODE_GET_KEY(node, &temp_mv);
				MV_FORCE_STRD(&temp_mv);
				YDB_MALLOC_BUFFER(&subscripts[cnt_fmt - 1 - cnt], temp_mv.str.len);
				subscripts[cnt_fmt - 1 - cnt].len_used = temp_mv.str.len;
				memcpy(subscripts[cnt_fmt - 1 - cnt].buf_addr, temp_mv.str.addr, temp_mv.str.len);
				/* we have now copied the subscript for ydb_encode_s() */
			}
			cnt_save = cnt_fmt;
			status = ydb_encode_s(&variable, cnt_fmt, subscripts, format, &zyencode_ret);
			gvname_env_restore(gblp1);
			if (outofband)
				async_action(FALSE);
			if (YDB_OK != status)
			{
				for (int i = 0; i < cnt_save; i++)
					YDB_FREE_BUFFER(&subscripts[i]);
				POP_MV_STENT();	/* value */
				zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
				ENCODE_DECODE_NESTED_RTS_ERROR(status, ERR_ZYENCODEINCOMPL);
			}
			key_end = gv_currkey->end;
			address = zyencode_ret.address;
			length = zyencode_ret.length;
			rec_size = gblp1->s_gv_cur_region->max_rec_size;
			cnt = 1;
			for (unsigned long remain = length; 0 != remain; remain -= value->str.len, cnt++)
			{
				tmp_mval.mvtype = MV_STR;
				tmp_mval.str.addr = (char *)buff;
				tmp_mval.str.len = SNPRINTF(tmp_mval.str.addr, MAX_NUM_SIZE, "%d", cnt);
				mval2subsc(&tmp_mval, gv_currkey, gv_cur_region->std_null_coll);
				value->mvtype = MV_STR;
				value->str.len = (rec_size < length) ? rec_size : length;
				value->str.addr = address;
				op_gvput(value);
				gv_currkey->end = key_end;
				length -= value->str.len;
				address += value->str.len;
			}
			gv_currkey->base[key_end] = KEY_DELIMITER;
			value->str.addr = (char *)buff;
			value->str.len = SNPRINTF(value->str.addr, MAX_NUM_SIZE, "%d", --cnt);
			op_gvput(value);
			gvname_env_restore(gblp1);	/* store destination as naked indicator in gv_currkey */
			/* zyencode_ret.address was returned by Jansson in ydb_encode_s(), which used the system malloc() */
			system_free(zyencode_ret.address);	/* free after we store the chunk number in op_gvput */
			zyencode_ret.address = NULL;	/* Need to set NULL because this handler can be called deep in the stack,
							 * and there  is no other way to ensure no double free() can happen,
							 * other than always setting it NULL after it is free()'d.
							 */
			zyencode_ret.length = 0;
			for (int i = 0; i < cnt_save; i++)
				YDB_FREE_BUFFER(&subscripts[i]);
		}
	}
	POP_MV_STENT();	/* value */
	zyencode_args = 0;	/* Must reset to zero to reuse zyencode */
}
