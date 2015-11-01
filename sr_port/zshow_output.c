/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "subscript.h"
#include "rtnhdr.h"
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

void zshow_output(zshow_out *out, const mstr *str)
{
	mval		*mv, lmv;
	lv_val		*temp;
	char		buff, *strptr, *strnext, *strtop, *strbase, *leadptr;
	unsigned char	*kend, kbuff[MAX_ZWR_KEY_SZ];
	int		key_ovrhd, len, outlen, buff_len, str_processed, chcnt, char_len, disp_len, n_spaces;
	int		device_width, inchar_width, cumul_width;
	boolean_t	utf8_active;
#ifdef UNICODE_SUPPORTED
	wint_t		codepoint;
#endif

	error_def(ERR_ZSHOWGLOSMALL);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	if (NULL != str)
	{
		buff_len = out->ptr - out->buff;
		out->size = MAXSTR_BUFF_ALLOC(str->len, out->buff, buff_len);
		out->ptr = out->buff + buff_len;
	}
	switch (out->type)
	{
	case ZSHOW_DEVICE:
		if (!process_exiting)
		{	/* Only if not exiting in case we are called from mdb_condition_handler
			 * with stack overflow */
			PUSH_MV_STENT(MVST_MVAL);
			mv = &mv_chain->mv_st_cont.mvs_mval;
		} else
			mv = &lmv;
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			char_len = UNICODE_ONLY((gtm_utf8_mode) ? UTF8_LEN_STRICT(strptr, len) :) len;
			disp_len = UNICODE_ONLY((gtm_utf8_mode) ? gtm_wcswidth(strptr, len, FALSE, 1) :) len;
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
				else {
					utf8_active = (CHSET_M != io_curr_device.out->ichset); /* needed by GTM_IO_WCWIDTH macro */
					cumul_width = out->displen;
					for (chcnt = 0; chcnt < char_len; ++chcnt)
					{
						strnext = UTF8_MBTOWC(strptr, strtop, codepoint);
						GTM_IO_WCWIDTH(codepoint, inchar_width);
						if ((cumul_width + inchar_width) > device_width)
							break;
						cumul_width += inchar_width;
						disp_len -= inchar_width;
						strptr = strnext;
					}
					outlen = strptr - strbase;
				}
#endif
				memcpy(out->ptr, strbase, outlen);
				out->ptr += outlen;
				str_processed += outlen;
				len = strtop - strptr;
				char_len -= chcnt;
				assert((UNICODE_ONLY((gtm_utf8_mode) ? UTF8_LEN_STRICT(strptr, len) :) len) == char_len);
				mv->str.addr = out->buff;
				mv->str.len = out->ptr - out->buff;
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
			out->len += char_len;
			out->displen += disp_len;
		}
		if (out->flush && out->ptr != out->buff)
		{
			mv->str.addr = out->buff;
			mv->str.len = out->ptr - out->buff;
			mv->mvtype = MV_STR;
			op_write(mv);
			op_wteol(1);
			out->ptr = out->buff;
			out->len = 0;
			out->displen = 0;
			out->line_num++;
		}
		if (!process_exiting)
		{
			POP_MV_STENT();
		}
		break;
	case ZSHOW_LOCAL:
		if (!process_exiting)
		{
			PUSH_MV_STENT(MVST_MVAL);
			mv = &mv_chain->mv_st_cont.mvs_mval;
		} else
			mv = &lmv;

		if (out->code && out->code != out->curr_code)
		{
			if (stringpool.top - stringpool.free < 1)
				stp_gcol(1);
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = 1;
			mv->mvtype = MV_STR;
			*mv->str.addr = out->code;
			stringpool.free++;
			if ((out->out_var.lv.child = op_srchindx(VARLSTCNT(2) out->out_var.lv.lvar, mv)))
			{
				bool	lvundef;

				lvundef = FALSE;
				if (!MV_DEFINED(&out->out_var.lv.lvar->v))
				{
					out->out_var.lv.lvar->v.mvtype = MV_STR;
					out->out_var.lv.lvar->v.str.len = 0;
					lvundef = TRUE;
				}
				op_kill(out->out_var.lv.child);
				if (lvundef)
					out->out_var.lv.lvar->v.mvtype = 0;
			}
			/* Check if we can add two more subscripts */
			if (MV_SBS == out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->ident &&
				MAX_LVSUBSCRIPTS <= out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->level + 2)
				rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
			out->out_var.lv.child = op_putindx(VARLSTCNT(2) out->out_var.lv.lvar, mv);
		}
		if (str)
		{
			len = str->len;
			strptr = str->addr;
			str_processed = 0;
		}
		if (str && ((int)str->len + (out->ptr - out->buff) > MAX_SRCLINE))
		{
			strtop = str->addr + str->len;
			for (; strptr != strtop; )
			{
				len = strtop - strptr;
				if (len <= MAX_SRCLINE - (out->ptr - out->buff))
					break;
				len = MAX_SRCLINE - (out->ptr - out->buff);
				strbase = str->addr + str_processed;
#ifdef UNICODE_SUPPORTED
				if (gtm_utf8_mode)
				{ /* terminate at the proper character boundary within MAX_SRCLINE bytes */
					UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
					len = leadptr - strbase;
				}
#endif
				memcpy(out->ptr, strbase, len);
				strptr += len;
				out->ptr += len;
				str_processed += len;
				mv->str.addr = 0;
				mv->str.len = 0;
				MV_FORCE_MVAL(mv, out->line_num);
				/* Check if we can add one more subscripts */
				if (MV_SBS == out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->ident &&
					MAX_LVSUBSCRIPTS <= out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->level + 1)
					rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
				temp = op_putindx(VARLSTCNT(2) out->out_var.lv.child, mv);
				if (stringpool.top - stringpool.free < out->ptr - out->buff)
					stp_gcol(out->ptr - out->buff);
				mv->mvtype = MV_STR;
				mv->str.addr = (char *)stringpool.free;
				mv->str.len = out->ptr - out->buff;
				memcpy(mv->str.addr, &out->buff[0], mv->str.len);
				stringpool.free += mv->str.len;
				temp->v = *mv;
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
			mv->str.addr = 0;
			mv->str.len = 0;
			MV_FORCE_MVAL(mv, out->line_num);
			/* Check if we can add one more subscripts */
			if (MV_SBS == out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->ident &&
				MAX_LVSUBSCRIPTS <= out->out_var.lv.lvar->ptrs.val_ent.parent.sbs->level + 1)
				rts_error(VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
			temp = op_putindx(VARLSTCNT(2) out->out_var.lv.child, mv);
			if (stringpool.top - stringpool.free < out->ptr - out->buff)
				stp_gcol(out->ptr - out->buff);
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = out->ptr - out->buff;
			mv->mvtype = MV_STR;
			memcpy(mv->str.addr, &out->buff[0], mv->str.len);
			stringpool.free += mv->str.len;
			temp->v = *mv;
			out->ptr = out->buff;
			out->line_num++;
		}
		if (!process_exiting)
		{
			POP_MV_STENT();
		}
		break;
	case ZSHOW_GLOBAL:
		if (!out->len)
		{
			key_ovrhd = gv_currkey->end + 1 + F_SUBSC_LEN + N_SUBSC_LEN;
			out->len = gv_cur_region->max_rec_size - key_ovrhd - sizeof(rec_hdr);
			if (out->len < MIN_DATASIZE)
				rts_error(VARLSTCNT(1) ERR_ZSHOWGLOSMALL);
		}
		if (!process_exiting)
		{
			PUSH_MV_STENT(MVST_MVAL);
			mv = &mv_chain->mv_st_cont.mvs_mval;
		} else
			mv = &lmv;
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
			if (gv_currkey->end + 1 > gv_cur_region->max_key_size)
			{
				if (0 == (kend = format_targ_key(kbuff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
					kend = &kbuff[MAX_ZWR_KEY_SZ - 1];
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, kend - kbuff, kbuff);
			}
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
				len = strtop - strptr;
				if (len <= out->len - (out->ptr - out->buff))
					break;
				len = out->len - (out->ptr - out->buff);
				strbase = str->addr + str_processed;
#ifdef UNICODE_SUPPORTED
				if (gtm_utf8_mode)
				{ /* terminate at the proper character boundary within out->len bytes */
					UTF8_LEADING_BYTE(strbase + len, strbase, leadptr);
					len = leadptr - strbase;
				}
#endif
				memcpy(out->ptr, strbase, len);
				strptr += len;
				out->ptr += len;
				str_processed += len;
				MV_FORCE_MVAL(mv, out->line_num);
				if (out->line_num != 1)
					op_gvnaked(VARLSTCNT(1) mv);
				else
				{
					mval2subsc(mv, gv_currkey);
					if (gv_currkey->end + 1 > gv_cur_region->max_key_size)
					{
						if (0 == (kend = format_targ_key(kbuff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
							kend = &kbuff[MAX_ZWR_KEY_SZ - 1];
						rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, kend - kbuff, kbuff);
					}
				}
				if (stringpool.top - stringpool.free < out->ptr - out->buff)
					stp_gcol(out->ptr - out->buff);
				mv->str.addr = (char *)stringpool.free;
				mv->str.len = out->ptr - out->buff;
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
			if (out->line_num != 1)
				op_gvnaked(VARLSTCNT(1) mv);
			else
			{
				mval2subsc(mv, gv_currkey);
				if (gv_currkey->end + 1 > gv_cur_region->max_key_size)
				{
					if (0 == (kend = format_targ_key(kbuff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
						kend = &kbuff[MAX_ZWR_KEY_SZ - 1];
					rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, kend - kbuff, kbuff);
				}
			}
			if (stringpool.top - stringpool.free < out->ptr - out->buff)
				stp_gcol(out->ptr - out->buff);
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = out->ptr - out->buff;
			mv->mvtype = MV_STR;
			memcpy(mv->str.addr, &out->buff[0], mv->str.len);
			stringpool.free += mv->str.len;
			op_gvput(mv);
			out->ptr = out->buff;
			out->line_num++;
		}
		if (!process_exiting)
		{
			POP_MV_STENT();
		}
		break;
	default:
		GTMASSERT;
		break;
	}
	out->curr_code = out->code;
	out->flush = 0;
}
