/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <devdef.h>
#include <rms.h>

#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "stringpool.h"
#include "copy.h"

typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF unsigned char io_params_size[];

void iorm_use(io_desc *iod, mval *pp)
{
	unsigned char	c;
	int4		width, length, blocksize;
	int4		status;
	d_rm_struct	*rm_ptr;
	struct RAB	*r;
	struct FAB	*f;
	int 		p_offset;
	boolean_t	shared_seen = FALSE;

	error_def(ERR_DEVPARMNEG);
	error_def(ERR_RMWIDTHPOS);
	error_def(ERR_RMWIDTHTOOBIG);
	error_def(ERR_RMNOBIGRECORD);
	error_def(ERR_RMBIGSHARE);
	error_def(ERR_MTBLKTOOBIG);
	error_def(ERR_MTBLKTOOSM);

	p_offset = 0;
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	r  = &rm_ptr->r;
	f  = &rm_ptr->f;
	assert(r->rab$l_fab == f);
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		assert(*(pp->str.addr + p_offset) < n_iops);
		switch ((c = *(pp->str.addr + p_offset++)))
		{
		case iop_allocation:
			if (iod->state != dev_open)
				f->fab$l_alq = *(int4*)(pp->str.addr + p_offset);
			break;
		case iop_append:
			if (iod->state != dev_open)
				r->rab$l_rop |= RAB$M_EOF;
			break;
		case iop_blocksize:
			if (iod->state != dev_open)
			{
				GET_LONG(blocksize, pp->str.addr + p_offset);
				if (MAX_RMS_ANSI_BLOCK < blocksize)
					rts_error(VARLSTCNT(1) ERR_MTBLKTOOBIG);
				else if (MIN_RMS_ANSI_BLOCK > blocksize)
					rts_error(VARLSTCNT(3) ERR_MTBLKTOOSM, 1, MIN_RMS_ANSI_BLOCK);
				else
					f->fab$w_bls = (unsigned short)blocksize;
			}
			break;
		case iop_contiguous:
			if (iod->state != dev_open)
			{
				f->fab$l_fop &= ~FAB$M_CBT;
				f->fab$l_fop |= FAB$M_CTG;
			}
			break;
		case iop_delete:
			f->fab$l_fop |= FAB$M_DLT;
			break;
		case iop_extension:
			GET_USHORT(f->fab$w_deq, pp->str.addr + p_offset);
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = pp->str.addr + p_offset + 1;
			s2pool(&iod->error_handler);
			break;
		case iop_fixed:
			if (iod->state != dev_open)
				rm_ptr->f.fab$b_rfm = rm_ptr->b_rfm = FAB$C_FIX;
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_newversion:
			if (iod->state != dev_open)
			{
				f->fab$l_fop |= FAB$M_MXV;
				f->fab$l_fop &= ~(FAB$M_CIF | FAB$M_SUP);
			}
			break;
		case iop_nosequential:
			break;
		case iop_s_protection:
			rm_ptr->promask &= ~(0x0F << XAB$V_SYS);
			rm_ptr->promask |= ((~(unsigned char)*(pp->str.addr + p_offset) & 0x0000000F) << XAB$V_SYS);
			break;
		case iop_w_protection:
			rm_ptr->promask &= ~(0x0F << XAB$V_WLD);
			rm_ptr->promask |= ((~(unsigned char)*(pp->str.addr + p_offset) & 0x0000000F) << XAB$V_WLD);
			break;
		case iop_g_protection:
			rm_ptr->promask &= ~(0x0F << XAB$V_GRP);
			rm_ptr->promask |= ((~(unsigned char)*(pp->str.addr + p_offset) & 0x0000000F) << XAB$V_GRP);
			break;
		case iop_o_protection:
			rm_ptr->promask &= ~(0x0F << XAB$V_OWN);
			rm_ptr->promask |= ((~(unsigned char)*(pp->str.addr + p_offset) & 0x0000000F) << XAB$V_OWN);
			break;
		case iop_readonly:
			if (iod->state != dev_open)
				f->fab$b_fac = FAB$M_GET;
			break;
		case iop_noreadonly:
			if (iod->state != dev_open)
				f->fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_TRN;
			break;
		case iop_recordsize:
			if (iod->state != dev_open)
			{
				GET_LONG(width, pp->str.addr + p_offset);
				if (width <= 0)
					rts_error(VARLSTCNT(1) ERR_RMWIDTHPOS);
				iod->width = width;
				if (MAX_RMS_RECORDSIZE >= width)
					r->rab$w_usz = f->fab$w_mrs = (unsigned short)width;
				else if (MAX_STRLEN < width)
					rts_error(VARLSTCNT(1) ERR_RMWIDTHTOOBIG);
				else if (!rm_ptr->largerecord)
					rts_error(VARLSTCNT(1) ERR_RMNOBIGRECORD);
				rm_ptr->l_usz = rm_ptr->l_mrs = width;
			}
			break;
		case iop_shared:
			if (iod->state != dev_open)
				shared_seen = TRUE;
			break;
		case iop_spool:
			f->fab$l_fop |= FAB$M_SPL;
			break;
		case iop_submit:
			f->fab$l_fop |= FAB$M_SCF;
			break;
		case iop_rfa:
			break;
		case iop_space:
			if (iod->state == dev_open && f->fab$l_dev & DEV$M_SQD)
			{
				GET_LONG(r->rab$l_bkt, pp->str.addr + p_offset);
				if ((status = sys$space(r, 0, 0)) != RMS$_NORMAL)
					rts_error(VARLSTCNT(1) status);
				r->rab$l_bkt = 0;
			}
			break;
		case iop_uic:
		{
			unsigned char	*ch, ct, *end;
			uic_struct	uic;
			struct XABPRO	*xabpro;

			ch = pp->str.addr + p_offset;
			ct = *ch++;
			end = ch + ct;
			uic.grp = uic.mem = 0;
			xabpro = malloc(SIZEOF(struct XABPRO));
			*xabpro = cc$rms_xabpro;
/* g,m are octal - no matter currently since iorm_open overwrites fab xab */
			while (*ch != ',' &&	ch < end)
				uic.grp = (10 * uic.grp) + (*ch++ - '0');
			if (*ch == ',')
			{
				while (++ch < end)
					uic.mem = (10 * uic.mem) + (*ch - '0');
			}
			xabpro->xab$l_uic = *((int4 *)&uic);
			f->fab$l_xab = xabpro;
			break;
		}
		case iop_width:
			if (iod->state == dev_open)
			{
				GET_LONG(width, pp->str.addr + p_offset);
				if (width <= 0)
					rts_error(VARLSTCNT(1) ERR_RMWIDTHPOS);
				else  if (width <= rm_ptr->l_mrs)
				{
					iorm_flush(iod);
					rm_ptr->l_usz = iod->width = width;
					if (!rm_ptr->largerecord)
						r->rab$w_usz = (short)width;
					iod->wrap = TRUE;
				}
			}
			break;
		case iop_wrap:
			iod->wrap = TRUE;
			break;
		case iop_nowrap:
			iod->wrap = FALSE;
			break;
		case iop_convert:
			r->rab$l_rop |= RAB$M_CVT;
			break;
		case iop_rewind:
			if (iod->state == dev_open && rm_ptr->f.fab$l_dev & DEV$M_FOD)
			{
				if (iod->dollar.zeof && rm_ptr->outbuf_pos > rm_ptr->outbuf)
					iorm_wteol(1, iod);
				sys$rewind(r);
				iod->dollar.zeof = FALSE;
				iod->dollar.y = 0;
				iod->dollar.x = 0;
				rm_ptr->outbuf_pos = rm_ptr->outbuf;
				rm_ptr->r.rab$l_ctx = FAB$M_GET;
			}
			break;
		case iop_truncate:
			r->rab$l_rop |= RAB$M_TPT;
			break;
		case iop_notruncate:
			r->rab$l_rop &= ~RAB$M_TPT;
			break;
		case iop_bigrecord:
			if (iod->state != dev_open)
				rm_ptr->largerecord = TRUE;
			break;
		case iop_nobigrecord:
			if (iod->state != dev_open)
			{
				if (MAX_RMS_RECORDSIZE < rm_ptr->l_mrs)
					rts_error(ERR_RMNOBIGRECORD);
				rm_ptr->largerecord = FALSE;
			}
			break;
		case iop_rfm:
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}
	if (shared_seen)
	{
		f->fab$b_shr = FAB$M_SHRGET;
		if (rm_ptr->largerecord)
		{
			if (f->fab$b_fac & FAB$M_PUT)
			{
				rts_error(VARLSTCNT(1) ERR_RMBIGSHARE);
			}
		} else if ((f->fab$b_fac & FAB$M_PUT) == FALSE)
			f->fab$b_shr |= FAB$M_SHRPUT;
	}
}/* eor */
