/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
#include "gtm_stdio.h"
#include "min_max.h"

#include "error.h"
#include "lv_val.h"
#include "subscript.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "mlkdef.h"
#include "zshow.h"
#include "compiler.h"
#include "io.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "op.h"
#include "mvalconv.h"
#include "format_targ_key.h"
#include "gtm_maxstr.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif
#include "gtmimagename.h"

#define F_SUBSC_LEN		 3
#define N_SUBSC_LEN		 5

#define WRITE_ONE_LINE_FROM_BUFFER			\
{							\
	mv->str.addr = out->buff;			\
	mv->str.len = INTCAST(out->ptr - out->buff);	\
	mv->mvtype = MV_STR;				\
	op_write(mv);					\
	op_wteol(1);					\
	out->ptr = out->buff;				\
	out->len = 0;					\
	out->displen = 0;				\
	out->line_num++;				\
}

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	gv_key		*gv_currkey;
GBLREF	gd_region	*gv_cur_region;
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char    *msp, *stackwarn, *stacktop;
GBLREF	int		process_exiting;
GBLREF	volatile boolean_t	timer_in_handler;

LITREF mval literal_null;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);
error_def(ERR_MAXNRSUBSCRIPTS);

void zshow_output(zshow_out *out, const mstr *str)
{
	boolean_t	is_base_var, lvundef, utf8_active, zshow_depth;
	char		buff, *leadptr, *piecestr, *strbase, *strnext, *strptr, *strtokptr, *strtop, *tempstr;
	gd_addr		*gbl_gd_addr;
	gvnh_reg_t	*gvnh_reg;
	int		dbg_sbs_depth, sbs_depth, str_processed;
	lv_val		*lv, *lv_child;
	mval		lmv, *mv_child, *mv;
	ssize_t		buff_len, cumul_width, device_width, inchar_width, len, outlen, chcnt, char_len, disp_len;
#ifdef UNICODE_SUPPORTED
	wint_t		codepoint;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL != str)
	{
		buff_len = (int)(out->ptr - out->buff);
		out->size = MAXSTR_BUFF_ALLOC(str->len, out->buff, buff_len);
		out->ptr = out->buff + buff_len;
	}
	if (!process_exiting)
	{	/* Only if not exiting in case we are called from mdb_condition_handler
		 * with stack overflow */
		PUSH_MV_STENT(MVST_MVAL);
		mv = &mv_chain->mv_st_cont.mvs_mval;
	} else
		mv = &lmv;
	mv->mvtype = 0; /* initialize mval in M-stack in case stp_gcol gets called before value gets initialized below */
	/* does this zshow "code" use subscripts for output */
	if ((('C' == out->code) || ('c' == out->code)) && ((ZSHOW_LOCAL == out->type) || (ZSHOW_GLOBAL == out->type)))
	{
		zshow_depth = TRUE;
		/* get a pointer to a garbage collection protected mval */
		PUSH_MV_STENT(MVST_MVAL);
		mv_child = &mv_chain->mv_st_cont.mvs_mval;
		mv_child->mvtype = MV_STR;
		ENSURE_STP_FREE_SPACE(1);
		/* create an mval for the "code" subscript */
		mv_child->str.addr = (char *)stringpool.free;
		*mv_child->str.addr = out->code;
		mv_child->str.len = 1;
		stringpool.free +=1;
	}
	else
		zshow_depth = FALSE;
	switch (out->type)
	{
	case ZSHOW_DEVICE:
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			char_len = UNICODE_ONLY((gtm_utf8_mode) ? (ssize_t)UTF8_LEN_STRICT(strptr, (int)len) :) len;
			disp_len = UNICODE_ONLY((gtm_utf8_mode) ?
						(ssize_t)gtm_wcswidth((unsigned char *)strptr, (int)len, FALSE, 1) :) len;
			str_processed = 0;
		}
		device_width = io_curr_device.out->width;
		if (str)
		{
			for (; (len > 0) && (disp_len + out->displen > device_width); )
			{
				assert(len <= out->size - (out->ptr - out->buff));
				strbase = strptr = str->addr + str_processed;
				strtop = str->addr + str->len;
				if (!gtm_utf8_mode)
				{
					outlen = device_width - out->len;
					chcnt = outlen;
					strptr += outlen;
					disp_len -= outlen;
				}
#				ifdef UNICODE_SUPPORTED
				else
				{
					utf8_active = (CHSET_M != io_curr_device.out->ichset); /* needed by GTM_IO_WCWIDTH macro */
					cumul_width = out->displen;
					for (chcnt = 0; chcnt < char_len; ++chcnt)
					{
						strnext = (char *)UTF8_MBTOWC(strptr, strtop, codepoint);
						GTM_IO_WCWIDTH(codepoint, inchar_width);
						if ((cumul_width + inchar_width) > device_width)
							break;
						cumul_width += inchar_width;
						disp_len -= inchar_width;
						strptr = strnext;
					}
					outlen = (ssize_t)(strptr - strbase);
				}
#				endif
				memcpy(out->ptr, strbase, outlen);
				out->ptr += outlen;
				str_processed += (int)outlen;
				len = (ssize_t)(strtop - strptr);
				char_len -= chcnt;
				assert((UNICODE_ONLY((gtm_utf8_mode) ?
						     (ssize_t)UTF8_LEN_STRICT(strptr, (int)len) :) len) == char_len);
				WRITE_ONE_LINE_FROM_BUFFER;
			}
			memcpy(out->ptr, str->addr + str_processed, len);
			out->ptr += len;
			out->len += (int)char_len;
			out->displen += (int)disp_len;
		}
		if (out->flush && out->ptr != out->buff)
			WRITE_ONE_LINE_FROM_BUFFER
		break;
	case ZSHOW_LOCAL:
		if (out->code) /* For locals, code == 0 indicates there is nothing to add (str) nor to flush */
		{
			if (zshow_depth)
			{
				/* if the subscript "code" already exists, delete it */
				lv = out->out_var.lv.lvar;
				if (out->code != out->curr_code)
				{
					lv_child = op_srchindx(VARLSTCNT(2) lv, mv_child);
					if (NULL != lv_child)
					{
						lvundef = FALSE;
						if (!LV_IS_VAL_DEFINED(lv))
						{
							lv->v.mvtype = MV_STR;
							lv->v.str.len = 0;
							lvundef = TRUE;
						}
						op_kill(lv_child);
						if (lvundef)
							lv->v.mvtype = 0;
					}
				}
				/* make sure another subscript will fit */
				is_base_var = LV_IS_BASE_VAR(lv);
				LV_SBS_DEPTH(lv, is_base_var, sbs_depth);
				if (MAX_LVSUBSCRIPTS <= (sbs_depth + 1))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				/* add the subscript for the "code" */
				lv_child = op_putindx(VARLSTCNT(2) lv, mv_child);
				lv_child->v.mvtype = 0; /* don't want a node so make it undef'd */
				for (tempstr = str->addr; piecestr = STRTOK_R(tempstr,".", &strtokptr);
						tempstr = NULL) /* WARNING assignment in test */
				{
					len = MIN(strlen(piecestr), MAX_MIDENT_LEN);
					/* create the mval for the next subscript */
					ENSURE_STP_FREE_SPACE(len);
					mv_child->str.addr = (char *)stringpool.free;
					stringpool.free +=len;
					strncpy(mv_child->str.addr, piecestr, len);
					mv_child->str.len = len;
					/* make sure the subscript will fit */
					is_base_var = LV_IS_BASE_VAR(lv_child);
					LV_SBS_DEPTH(lv_child, is_base_var, sbs_depth);
					if (MAX_LVSUBSCRIPTS <= (sbs_depth + 1))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
					/* add the subscript */
					lv_child = op_putindx(VARLSTCNT(2) lv_child, mv_child);
					lv_child->v.mvtype = 0; /* if it is not the last one, no node */
				}
				lv_child->v = literal_null; /* make a node out of the last one with a value of "" */
				POP_MV_STENT(); /* we are done with our mval */
				break;
			}
			if (out->code != out->curr_code)
			{
				ENSURE_STP_FREE_SPACE(1);
				mv->str.addr = (char *)stringpool.free;
				mv->str.len = 1;
				mv->mvtype = MV_STR;
				*mv->str.addr = out->code;
				stringpool.free++;
				lv = out->out_var.lv.lvar;
				out->out_var.lv.child = lv_child = op_srchindx(VARLSTCNT(2) lv, mv);
				if (NULL != lv_child)
				{
					lvundef = FALSE;
					if (!LV_IS_VAL_DEFINED(lv))
					{
						lv->v.mvtype = MV_STR;
						lv->v.str.len = 0;
						lvundef = TRUE;
					}
					op_kill(lv_child);
					if (lvundef)
						lv->v.mvtype = 0;
				}
				/* Check if we can add two more subscripts 1) out->code & 2) out->line_num */
				is_base_var = LV_IS_BASE_VAR(lv);
				LV_SBS_DEPTH(lv, is_base_var, sbs_depth);
				if (MAX_LVSUBSCRIPTS <= (sbs_depth + 2))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				out->out_var.lv.child = op_putindx(VARLSTCNT(2) lv, mv);
				DEBUG_ONLY(LV_SBS_DEPTH(out->out_var.lv.child, FALSE, dbg_sbs_depth);)
				assert(MAX_LVSUBSCRIPTS > (dbg_sbs_depth + 1));
			}
			if (str)
			{
				len = str->len;
				strptr = str->addr;
				str_processed = 0;
				if (len + (out->ptr - out->buff) > MAX_SRCLINE)
				{
					strtop = str->addr + str->len;
					lv_child = out->out_var.lv.child;
					assert(NULL != lv_child);
					for (; strptr != strtop; )
					{
						len = (ssize_t)(strtop - strptr);
						if (len <= MAX_SRCLINE - (out->ptr - out->buff))
							break;
						len = MAX_SRCLINE - (ssize_t)(out->ptr - out->buff);
						strbase = str->addr + str_processed;
#						ifdef UNICODE_SUPPORTED
						if (gtm_utf8_mode)
						{ /* terminate at the proper character boundary within MAX_SRCLINE bytes */
							UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
							len = (ssize_t)(leadptr - strbase);
						}
#						endif
						memcpy(out->ptr, strbase, len);
						strptr += len;
						out->ptr += len;
						str_processed += (int)len;
						mv->str.addr = 0;
						mv->str.len = 0;
						MV_FORCE_MVAL(mv, out->line_num);
						/* It is safe to add the second subscript here since the check for this
						 * is already done in the previous if block (MAX_LVSUBSCRIPTS error)
						 */
						DEBUG_ONLY(LV_SBS_DEPTH(lv_child, FALSE, dbg_sbs_depth);)
						assert(MAX_LVSUBSCRIPTS > (dbg_sbs_depth + 1));
						lv = op_putindx(VARLSTCNT(2) lv_child, mv);
						ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
						mv->mvtype = MV_STR;
						mv->str.addr = (char *)stringpool.free;
						mv->str.len = INTCAST(out->ptr - out->buff);
						memcpy(mv->str.addr, &out->buff[0], mv->str.len);
						stringpool.free += mv->str.len;
						lv->v = *mv;
						out->ptr = out->buff;
						out->line_num++;
					}
				}
				memcpy(out->ptr, str->addr + str_processed, len);
				out->ptr += len;
			}
			if (out->flush && (out->ptr != out->buff))
			{
				mv->str.addr = 0;
				mv->str.len = 0;
				MV_FORCE_MVAL(mv, out->line_num);
				/* Check for if it is ok to add this second subscript is already done above (MAX_LVSUBSCRIPTS) */
				lv_child = out->out_var.lv.child;
				assert(NULL != lv_child);
				DEBUG_ONLY(LV_SBS_DEPTH(lv_child, FALSE, dbg_sbs_depth);)
				assert(MAX_LVSUBSCRIPTS > (dbg_sbs_depth + 1));
				lv = op_putindx(VARLSTCNT(2) lv_child, mv);
				ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
				mv->str.addr = (char *)stringpool.free;
				mv->str.len = INTCAST(out->ptr - out->buff);
				mv->mvtype = MV_STR;
				memcpy(mv->str.addr, &out->buff[0], mv->str.len);
				stringpool.free += mv->str.len;
				lv->v = *mv;
				out->ptr = out->buff;
				out->line_num++;
			}
		}
		break;
	case ZSHOW_GLOBAL:
		if (zshow_depth)
		{
			gbl_gd_addr = TREF(gd_targ_addr);	/* set by op_gvname/op_gvextnam/op_gvnaked at start of ZSHOW cmd */
			gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set by op_gvname/op_gvextnam/op_gvnaked at start of ZSHOW cmd */
			gv_currkey->end = out->out_var.gv.end;
			gv_currkey->prev = out->out_var.gv.prev;
			gv_currkey->base[gv_currkey->end] = 0;
			/* add the "code" subscript */
			mval2subsc(mv_child, gv_currkey, gv_cur_region->std_null_coll);
			/* ensure this subscript "code" does not exist by deleting it*/
			if (out->code && out->code != out->curr_code)
			{
				GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
				if (gv_currkey->end >= gv_cur_region->max_key_size)
					ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
				op_gvkill();
			}
			tempstr=str->addr;
			/* build the key by adding the rest of the subscripts */
			for (tempstr = str->addr; piecestr = STRTOK_R(tempstr,".", &strtokptr); /* WARNING assignment in test */
					tempstr = NULL)
			{
				len = MIN(strlen(piecestr), MAX_MIDENT_LEN);
				ENSURE_STP_FREE_SPACE(len);
				mv_child->str.addr = (char *)stringpool.free;
				stringpool.free +=len;
				strncpy(mv_child->str.addr, piecestr, len);
				mv_child->str.len = len;
				mval2subsc(mv_child, gv_currkey, gv_cur_region->std_null_coll);
				GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
				if (gv_currkey->end >= gv_cur_region->max_key_size)
					ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
			}
			op_gvput((mval *)&literal_null); /* create the global */
			POP_MV_STENT(); /* we are done with our mval */
			break;
		}
		if (!out->len)
			out->len = (int)(gv_cur_region->max_rec_size);
		gbl_gd_addr = TREF(gd_targ_addr);	/* set by op_gvname/op_gvextnam/op_gvnaked at start of ZSHOW cmd */
		gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set by op_gvname/op_gvextnam/op_gvnaked at start of ZSHOW cmd */
		if (out->code && out->code != out->curr_code)
		{
			gv_currkey->end = out->out_var.gv.end;
			gv_currkey->prev = out->out_var.gv.prev;
			gv_currkey->base[gv_currkey->end] = 0;
			mv->mvtype = MV_STR;
			mv->str.len = 1;
			mv->str.addr = &buff;
			*mv->str.addr = out->code;
			mval2subsc(mv, gv_currkey, gv_cur_region->std_null_coll);
			/* If gvnh_reg corresponds to a spanning global, then determine
			 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
			 */
			GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
			if (gv_currkey->end >= gv_cur_region->max_key_size)
				ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
			assert(gv_currkey->end - 3 == out->out_var.gv.end);	/* true for current 1 character string codes */
			op_gvkill();
		}
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			str_processed = 0;
			if ((int)len + (out->ptr - out->buff) > out->len)
			{	/* won't fit in a single database record */
				for (strtop = str->addr + str->len; strptr != strtop; out->line_cont++)
				{	/* line_cont initialized to 0  by setup in op_zshow.c */
					len = (ssize_t)(strtop - strptr);
					if (len <= out->len - (out->ptr - out->buff))
					{
						assert(0 < out->line_cont);	/*  should not come into loop unless needed */
						break;
					}
					len = out->len - (ssize_t)(out->ptr - out->buff);
					assert(0 <= len);
					strbase = str->addr + str_processed;
#					ifdef UNICODE_SUPPORTED
					if (gtm_utf8_mode)
					{ /* terminate at the proper character boundary within out->len bytes */
						UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
						len = (ssize_t)(leadptr - strbase);
					}
#					endif
					memcpy(out->ptr, strbase, len);
					strptr += len;
					out->ptr += len;
					str_processed += (int)len;
					if (out->line_cont)
					{	/* continuations are all at the same level (established below) */
						MV_FORCE_MVAL(mv, out->line_cont);
						op_gvnaked(VARLSTCNT(1) mv);
					} else
					{
						MV_FORCE_MVAL(mv, out->line_num);
						if (NOT_FIRST_LINE_OF_ZSHOW_OUTPUT(out))
							op_gvnaked(VARLSTCNT(1) mv);
						else
						{
							mval2subsc(mv, gv_currkey, gv_cur_region->std_null_coll);
							/* If gvnh_reg corresponds to a spanning global, then determine
							 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
							 */
							GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
							if (gv_currkey->end >= gv_cur_region->max_key_size)
								ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
						}
					}
					ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
					mv->str.addr = (char *)stringpool.free;
					mv->str.len = INTCAST(out->ptr - out->buff);
					mv->mvtype = MV_STR;
					memcpy(mv->str.addr, &out->buff[0], mv->str.len);
					stringpool.free += mv->str.len;
					op_gvput(mv);
					stringpool.free = (unsigned char *)mv->str.addr;	/* prevent bloat in the loop */
					out->ptr = out->buff;
					if (!out->line_cont)
					{	/* the initial chunk went at the line_num right under the code */
						MV_FORCE_MVAL(mv, out->line_cont);	/* but the rest go down a level */
						mval2subsc(mv, gv_currkey, gv_cur_region->std_null_coll);
						GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
						if (gv_currkey->end >= gv_cur_region->max_key_size)
							ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
					}
				}
			}
			memcpy(out->ptr, str->addr + str_processed, len);
			out->ptr += len;
		}
		if (out->flush && out->ptr != out->buff)
		{
			if (out->line_cont)
			{	/* completing a continuation */
				MV_FORCE_MVAL(mv, out->line_cont);
				op_gvnaked(VARLSTCNT(1) mv);
			} else
			{	/* not finishing a continuation  */
				MV_FORCE_MVAL(mv, out->line_num);
				if (NOT_FIRST_LINE_OF_ZSHOW_OUTPUT(out))
					op_gvnaked(VARLSTCNT(1) mv);
				else
				{
					mval2subsc(mv, gv_currkey, gv_cur_region->std_null_coll);
					/* If gvnh_reg corresponds to a spanning global, then determine
					 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
					 */
					GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gbl_gd_addr, gv_currkey);
					if (gv_currkey->end >= gv_cur_region->max_key_size)
						ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
				}
			}
			ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = INTCAST(out->ptr - out->buff);
			mv->mvtype = MV_STR;
			memcpy(mv->str.addr, &out->buff[0], mv->str.len);
			stringpool.free += mv->str.len;
			op_gvput(mv);
			out->ptr = out->buff;
			if (out->line_cont)
			{	/* after a continuation reset the key up a level where the line_number goes */
				gv_currkey->end = gv_currkey->prev;
				gv_currkey->base[gv_currkey->end] = 0;
				gv_currkey->prev = out->out_var.gv.end + 3;	/* use condition asserted for 1 character codes */
				out->line_cont = 0;
			}
			out->line_num++;
		}
		break;
	case ZSHOW_BUFF_ONLY:
		/* This code is meant to print to a buffer so it DOES NOT bother setting the other fields of output.
		 * Only beginning and ending pointers because those are the only fields we need
		 */
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			memcpy(out->ptr, str->addr, len);
			out->ptr += len;
		}
		break;
	default:
		assertpro(FALSE && out->type);
		break;
	}
	if (!process_exiting)
		POP_MV_STENT();
	out->curr_code = out->code;
	out->flush = FALSE;
}
