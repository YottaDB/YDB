/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "hashdef.h"
#include "lv_val.h"
#include "subscript.h"
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

#define F_SUBSC_LEN 3
#define N_SUBSC_LEN 5
#define MIN_DATASIZE 40

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	gv_key		*gv_currkey;
GBLREF	gd_region	*gv_cur_region;
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char    *msp, *stackwarn, *stacktop;
GBLREF	int		process_exiting;

void zshow_output(zshow_out *out, mstr *str)
{
	mval		*mv, lmv;
	lv_val		*temp;
	char		buff, *ptr1, *ptr2;
	unsigned char	*kend, kbuff[MAX_KEY_SZ + 1];
	int		key_ovrhd, len;

	error_def(ERR_ZSHOWGLOSMALL);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	switch (out->type)
	{
	case ZSHOW_DEVICE:
		if (!process_exiting)
		{	/* Only if not exiting in case we are called from mdb_condition_handler
			   with stack overflwo */
			PUSH_MV_STENT(MVST_MVAL);
			mv = &mv_chain->mv_st_cont.mvs_mval;
		} else
			mv = &lmv;
		if (!out->len)
			out->len = io_curr_device.out->width;
		if (str)
		{
			len = str->len;
			ptr1 = str->addr;
		}
		if (str && ((int)str->len + (out->ptr - out->buff) > out->len))
		{
			ptr2 = str->addr + str->len;
			for (; ptr1 != ptr2; )
			{
				len = ptr2 - ptr1;
				if (len > out->len - (out->ptr - out->buff))
					len = out->len - (out->ptr - out->buff);
				else
					break;
				memcpy(out->ptr, ptr1, len);
				ptr1 += len;
				out->ptr += len;
				mv->str.addr = out->buff;
				mv->str.len = out->ptr - out->buff;
				mv->mvtype = MV_STR;
				op_write(mv);
				op_wteol(1);
				out->ptr = out->buff + 10;
				memset(out->buff, ' ', 10);
				out->line_num++;
			}
		}
		if (str)
		{
			memcpy(out->ptr, ptr1, len);
			out->ptr += len;
		}
		if (out->flush && out->ptr != out->buff)
		{
			mv->str.addr = out->buff;
			mv->str.len = out->ptr - out->buff;
			mv->mvtype = MV_STR;
			op_write(mv);
			op_wteol(1);
			out->ptr = out->buff;
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

		if (!out->len)
			out->len = MAX_SRCLINE;
		if (out->code && out->code != out->curr_code)
		{
			if (stringpool.top - stringpool.free < 1)
				stp_gcol(1);
			mv->str.addr = (char *)stringpool.free;
			mv->str.len = 1;
			mv->mvtype = MV_STR;
			*mv->str.addr = out->code;
			stringpool.free++;
			if (out->out_var.lv.child = op_srchindx(VARLSTCNT(2) out->out_var.lv.lvar, mv))
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
			ptr1 = str->addr;
		}
		if (str && ((int)str->len + (out->ptr - out->buff) > out->len))
		{
			ptr2 = str->addr + str->len;
			for (; ptr1 != ptr2; )
			{
				len = ptr2 - ptr1;
				if (len > out->len - (out->ptr - out->buff))
					len = out->len - (out->ptr - out->buff);
				else
					break;
				memcpy(out->ptr, ptr1, len);
				ptr1 += len;
				out->ptr += len;
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
			memcpy(out->ptr, ptr1, len);
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
				if (0 == (kend = format_targ_key(kbuff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
					kend = &kbuff[MAX_KEY_SZ];
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, kend - kbuff, kbuff);
			}
			op_gvkill();
		}
		if (str)
		{
			len = str->len;
			ptr1 = str->addr;
		}
		if (str && ((int)str->len + (out->ptr - out->buff) > out->len))
		{
			ptr2 = str->addr + str->len;
			for (; ptr1 != ptr2; )
			{
				len = ptr2 - ptr1;
				if (len > out->len - (out->ptr - out->buff))
					len = out->len - (out->ptr - out->buff);
				else
					break;
				memcpy(out->ptr, ptr1, len);
				ptr1 += len;
				out->ptr += len;
				MV_FORCE_MVAL(mv, out->line_num);
				if (out->line_num != 1)
					op_gvnaked(VARLSTCNT(1) mv);
				else
				{
					mval2subsc(mv, gv_currkey);
					if (gv_currkey->end + 1 > gv_cur_region->max_key_size)
					{
						if (0 == (kend = format_targ_key(kbuff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
							kend = &kbuff[MAX_KEY_SZ];
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
			memcpy(out->ptr, ptr1, len);
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
					if (0 == (kend = format_targ_key(kbuff, MAX_KEY_SZ + 1, gv_currkey, TRUE)))
						kend = &kbuff[MAX_KEY_SZ];
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
