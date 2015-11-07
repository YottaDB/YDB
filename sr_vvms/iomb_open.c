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

#include <descrip.h>
#include <dcdef.h>
#include <dvidef.h>
#include <iodef.h>
#include <jpidef.h>
#include <ssdef.h>
#include <efndef.h>


#include "io.h"
#include "io_params.h"
#include "iombdef.h"
#include "vmsdtype.h"
#include "stringpool.h"
#include "trans_log_name.h"
#include "copy.h"

#define MBX_FUNC_R (IO$_READVBLK)
#define MBX_FUNC_W (IO$_WRITEVBLK | IO$M_NOW)
#define TERM_READ_MIN_BCOUNT 1037
#define MBX_BCOUNT 1200
#define	UCB$V_PRMMBX	0	/* from $UCBDEF macro */

LITREF unsigned char io_params_size[];

short iomb_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	bool		create;
	unsigned char	buf1[MAX_TRANS_NAME_LEN];
	short		buf_ret, cls_ret, return_length, sts_ret;
	int4		byte_count, blocksize;
	uint4		devbuf, devclass, devsts, status;
	int		p_offset;
	d_mb_struct	*mb_ptr;
	io_desc		*ioptr;
	io_log_name	*tl;
	mstr		v, tn;
	params		ch;
	struct dsc$descriptor	lognam;
	struct	{
		item_list_3	item;
		int4		terminator;
		} getjpi_item_list = {4, JPI$_BYTCNT, &byte_count, &return_length, 0};
	struct	{
		item_list_3	item[3];
		int4		terminator;
		} getdvi_item_list = {4, DVI$_DEVSTS, &devsts, &sts_ret,
				4, DVI$_DEVCLASS, &devclass, &cls_ret,
				4, DVI$_DEVBUFSIZ, &devbuf, &buf_ret,
				0};

	error_def(ERR_INSFFBCNT);
	error_def(ERR_DEVPARMNEG);

	p_offset = 0;
	ioptr = dev_name->iod;
	if (ioptr->state == dev_never_opened)
	{
		ioptr->dev_sp = (d_mb_struct *)(malloc(SIZEOF(d_mb_struct)));
		mb_ptr = (d_mb_struct *)ioptr->dev_sp;
		mb_ptr->maxmsg = DEF_MB_MAXMSG;
		mb_ptr->promsk = 0;
		mb_ptr->del_on_close = FALSE;
		mb_ptr->prmflg = 0;
		mb_ptr->read_mask = MBX_FUNC_R;
		mb_ptr->write_mask = MBX_FUNC_W;
		ioptr->state = dev_closed;
	}
	mb_ptr = (d_mb_struct *)ioptr->dev_sp;
	if (ioptr->state != dev_open)
	{
		status = sys$getjpiw(EFN$C_ENF, 0, 0, &getjpi_item_list, 0, 0, 0);
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		if (byte_count < TERM_READ_MIN_BCOUNT + MBX_BCOUNT)
			rts_error(VARLSTCNT(1) ERR_INSFFBCNT);
		lognam.dsc$w_length	= (unsigned short)dev_name->len;
		lognam.dsc$b_dtype	= DSC$K_DTYPE_T;
		lognam.dsc$b_class	= DSC$K_CLASS_S;
		lognam.dsc$a_pointer	= dev_name->dollar_io;
		create = TRUE;
		status = sys$getdviw(EFN$C_ENF, 0, &lognam, &getdvi_item_list,
			0, 0, 0, 0);
		if ((status == SS$_NORMAL) && (devclass == DC$_MAILBOX))
			create = FALSE;
		while ((ch = *(pp->str.addr + p_offset++)) != iop_eol)
		{
			switch(ch)
			{
				case iop_blocksize:
					GET_LONG(blocksize, pp->str.addr + p_offset);
					if (blocksize < 0)
						rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
					mb_ptr->maxmsg = blocksize;
					break;
				case iop_delete:
					mb_ptr->del_on_close = TRUE;
					break;
				case iop_readonly:
					mb_ptr->promsk = IO_RD_ONLY;
					break;
				case iop_noreadonly:
					mb_ptr->promsk &= ~IO_RD_ONLY;
					break;
				case iop_writeonly:
					mb_ptr->promsk = IO_SEQ_WRT;
					break;
				case iop_nowriteonly:
					mb_ptr->promsk &= ~IO_SEQ_WRT;
					break;
				case iop_prmmbx:
					mb_ptr->prmflg = 1;
					break;
				case iop_tmpmbx:
					mb_ptr->prmflg = 0;
					break;
				case iop_exception:
					ioptr->error_handler.len = *(pp->str.addr + p_offset);
					ioptr->error_handler.addr = pp->str.addr + p_offset + 1;
					s2pool(&ioptr->error_handler);
					break;
				default:
					break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		if (create)
			status = sys$crembx(mb_ptr->prmflg
				,&(mb_ptr->channel)
				,mb_ptr->maxmsg
				,mb_ptr->maxmsg
				,mb_ptr->promsk
				,0
				,&lognam);
		else
		{
			status = sys$assign(&lognam, &(mb_ptr->channel), 0, 0);
			mb_ptr->maxmsg = devbuf;
			mb_ptr->prmflg = (devsts >> UCB$V_PRMMBX) & 1;
		}
		switch(status)
		{
			case SS$_NORMAL:
				v.addr = lognam.dsc$a_pointer;
				v.len = lognam.dsc$w_length;
				if ((status = trans_log_name(&v, &tn, buf1)) != SS$_NORMAL)
					rts_error(VARLSTCNT(1) status);
				tl = get_log_name(&tn, INSERT);
				tl->iod = dev_name->iod;
				tl->iod->trans_name = tl;
				break;
			case SS$_EXBYTLM:
			case SS$_EXPORTQUOTA:
			case SS$_INSFMEM:
			case SS$_INTERLOCK:
			case SS$_NOIOCHAN:
				return FALSE;
			default:
				rts_error(VARLSTCNT(1)  status);
		}
		ioptr->width = mb_ptr->maxmsg;
		if (mb_ptr->prmflg && mb_ptr->del_on_close)
		{
			if ((status = sys$delmbx(mb_ptr->channel)) != SS$_NORMAL)
				rts_error(VARLSTCNT(1)  status);
		}
		mb_ptr->in_pos = mb_ptr->in_top = mb_ptr->inbuf = malloc(ioptr->width);
		ioptr->length = DEF_MB_LENGTH;
		ioptr->state = dev_open;
	}
	return TRUE;
}
