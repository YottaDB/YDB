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

#include "mdef.h"

#include "gtm_string.h"

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

#define F_SUBSC_LEN		 3
#define N_SUBSC_LEN		 5
#define MIN_DATASIZE		40
#define	ZSHOW_SPACE_INDENT	10	/* # of spaces every following line of zshow output is indented with */

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	gv_key		*gv_currkey;
GBLREF	gd_region	*gv_cur_region;
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char    *msp, *stackwarn, *stacktop;
GBLREF	int		process_exiting;

error_def(ERR_ZSHOWGLOSMALL);
error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);
error_def(ERR_MAXNRSUBSCRIPTS);

void zshow_output(zshow_out *out, const mstr *str)
{
	mval		*mv, lmv;
	lv_val		*lv, *lv_child;
	char		buff, *strptr, *strnext, *strtop, *strbase, *leadptr;
	int		key_ovrhd, str_processed, n_spaces, sbs_depth, dbg_sbs_depth;
	ssize_t        	len, outlen, chcnt, char_len, disp_len ;
	int		buff_len;
	int		device_width, inchar_width, cumul_width;
	boolean_t	is_base_var, lvundef, utf8_active;
#ifdef UNICODE_SUPPORTED
	wint_t		codepoint;
#endif

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
		/* Do not indent if width is < the indentation length or if remaining width cannot accommodate one character */
		n_spaces = (device_width > (ZSHOW_SPACE_INDENT UNICODE_ONLY(+ (gtm_utf8_mode ? GTM_MB_DISP_LEN_MAX : 0)))
			? ZSHOW_SPACE_INDENT : 0);
		if (str && (disp_len + out->displen > device_width))
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
#ifdef UNICODE_SUPPORTED
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
#endif
				memcpy(out->ptr, strbase, outlen);
				out->ptr += outlen;
				str_processed += (int)outlen;
				len = (ssize_t)(strtop - strptr);
				char_len -= chcnt;
				assert((UNICODE_ONLY((gtm_utf8_mode) ?
						     (ssize_t)UTF8_LEN_STRICT(strptr, (int)len) :) len) == char_len);
				mv->str.addr = out->buff;
				mv->str.len = INTCAST(out->ptr - out->buff);
				mv->mvtype = MV_STR;
				op_write(mv);
				op_wteol(1);
				out->ptr = out->buff + n_spaces;
				memset(out->buff, ' ', n_spaces);
				out->len = n_spaces;
				out->displen = n_spaces;
				out->line_num++;
			}
		}
		if (str)
		{
			memcpy(out->ptr, str->addr + str_processed, len);
			out->ptr += len;
			out->len += (int)char_len;
			out->displen += (int)disp_len;
		}
		if (out->flush && out->ptr != out->buff)
		{
			mv->str.addr = out->buff;
			mv->str.len = INTCAST(out->ptr - out->buff);
			mv->mvtype = MV_STR;
			op_write(mv);
			op_wteol(1);
			out->ptr = out->buff;
			out->len = 0;
			out->displen = 0;
			out->line_num++;
		}
		break;
	case ZSHOW_LOCAL:
		if (out->code)
		{
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
					rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				out->out_var.lv.child = op_putindx(VARLSTCNT(2) lv, mv);
				DEBUG_ONLY(LV_SBS_DEPTH(out->out_var.lv.child, FALSE, dbg_sbs_depth);)
				assert(MAX_LVSUBSCRIPTS > (dbg_sbs_depth + 1));
			}
			if (str)
			{
				len = str->len;
				strptr = str->addr;
				str_processed = 0;
				if (str->len + (out->ptr - out->buff) > MAX_SRCLINE)
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
#ifdef UNICODE_SUPPORTED
						if (gtm_utf8_mode)
						{ /* terminate at the proper character boundary within MAX_SRCLINE bytes */
							UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
							len = (ssize_t)(leadptr - strbase);
						}
#endif
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
						out->ptr = out->buff + 10;
						memset(out->buff, ' ', 10);
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
		if (!out->len)
		{
			key_ovrhd = gv_currkey->end + 1 + F_SUBSC_LEN + N_SUBSC_LEN;
			out->len = (int)(gv_cur_region->max_rec_size - key_ovrhd - SIZEOF(rec_hdr));
			if (out->len < MIN_DATASIZE)
				rts_error(VARLSTCNT(1) ERR_ZSHOWGLOSMALL);
		}
		if (out->code && out->code != out->curr_code)
		{
			gv_currkey->end = out->out_var.gv.end;
			gv_currkey->prev = out->out_var.gv.prev;
			gv_currkey->base[gv_currkey->end] = 0;
			mv->mvtype = MV_STR;
			mv->str.len = 1;
			mv->str.addr = &buff;
			*mv->str.addr = out->code;
			mval2subsc(mv, gv_currkey);
			if (gv_currkey->end >= gv_cur_region->max_key_size)
				ISSUE_GVSUBOFLOW_ERROR(gv_currkey);
			op_gvkill();
		}
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			str_processed = 0;
		}
		if (str && ((int)str->len + (out->ptr - out->buff) > out->len))
		{
			strtop = str->addr + str->len;
			for (; strptr != strtop; )
			{
				len = (ssize_t)(strtop - strptr);
				if (len <= out->len - (out->ptr - out->buff))
					break;
				len = out->len - (ssize_t)(out->ptr - out->buff);
				strbase = str->addr + str_processed;
#ifdef UNICODE_SUPPORTED
				if (gtm_utf8_mode)
				{ /* terminate at the proper character boundary within out->len bytes */
					UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
					len = (ssize_t)(leadptr - strbase);
				}
#endif
				memcpy(out->ptr, strbase, len);
				strptr += len;
				out->ptr += len;
				str_processed += (int)len;
				MV_FORCE_MVAL(mv, out->line_num);
				if (FIRST_LINE_OF_ZSHOW_OUTPUT(out))
					op_gvnaked(VARLSTCNT(1) mv);
				else
				{
					mval2subsc(mv, gv_currkey);
					if (gv_currkey->end >= gv_cur_region->max_key_size)
						ISSUE_GVSUBOFLOW_ERROR(gv_currkey);
				}
				ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
				mv->str.addr = (char *)stringpool.free;
				mv->str.len = INTCAST(out->ptr - out->buff);
				mv->mvtype = MV_STR;
				memcpy(mv->str.addr, &out->buff[0], mv->str.len);
				stringpool.free += mv->str.len;
				op_gvput(mv);
				out->ptr = out->buff + 10;
				memset(out->buff, ' ', 10);
				out->line_num++;
			}
		}
		if (str)
		{
			memcpy(out->ptr, str->addr + str_processed, len);
			out->ptr += len;
		}
		if (out->flush && out->ptr != out->buff)
		{
			MV_FORCE_MVAL(mv, out->line_num);
			if (FIRST_LINE_OF_ZSHOW_OUTPUT(out))
				op_gvnaked(VARLSTCNT(1) mv);
			else
			{
				mval2subsc(mv, gv_currkey);
				if (gv_currkey->end >= gv_cur_region->max_key_size)
					ISSUE_GVSUBOFLOW_ERROR(gv_currkey);
			}
			ENSURE_STP_FREE_SPACE((int)(out->ptr - out->buff));
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = INTCAST(out->ptr - out->buff);
			mv->mvtype = MV_STR;
			memcpy(mv->str.addr, &out->buff[0], mv->str.len);
			stringpool.free += mv->str.len;
			op_gvput(mv);
			out->ptr = out->buff;
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
		GTMASSERT;
		break;
	}
	if (!process_exiting)
	{
		POP_MV_STENT();
	}
	out->curr_code = out->code;
	out->flush = 0;
}
