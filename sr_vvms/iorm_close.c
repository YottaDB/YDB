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

#include <rms.h>
#include "mdef.h"
#include "io.h"
#include "iormdef.h"
#include <ssdef.h>
#include <iodef.h>
#include <devdef.h>
#include "io_params.h"

LITREF unsigned char io_params_size[];

void iorm_close(io_desc *iod, mval *pp)
{
int4		status;
unsigned char	c;
int		p_offset;
d_rm_struct	*rm_ptr;
mstr		bque= {9,"SYS$BATCH"} ; 		/* default submit queue */
mstr		pque= {9,"SYS$PRINT"} ; 		/* default spool queue	*/
bool		rename, submit, spool, delete;
boolean_t	rm_destroy = TRUE;
boolean_t	rm_rundown = FALSE;

unsigned short	promask;
struct		FAB fab;
struct		NAM nam;
struct		XABPRO xab;

assert(iod != 0);
assert((pp->str.addr) != 0);
rm_ptr = (d_rm_struct *) iod->dev_sp;
if (iod->state == dev_open)
{
	promask = rm_ptr->promask;
	iorm_use(iod, pp);
	if ((rm_ptr->f.fab$b_fac == FAB$M_GET) == 0 && rm_ptr->r.rab$l_ctx == FAB$M_PUT &&
			rm_ptr->outbuf_pos != rm_ptr->outbuf && !iod->dollar.za)
	{	iorm_flush(iod);
	}
	p_offset = 0;
	delete = ((rm_ptr->f.fab$l_fop & FAB$M_DLT) == FAB$M_DLT) ;
	rename = submit = spool = FALSE ;
	if (rm_ptr->f.fab$l_dev & DEV$M_FOD)
	{
		while (*(pp->str.addr + p_offset) != iop_eol)
		{
			switch (c = *(pp->str.addr + p_offset++))
			{
			case iop_rename:
				fab = rm_ptr->f;
				fab.fab$b_fns = *(pp->str.addr + p_offset);
				fab.fab$l_fna = (pp->str.addr + p_offset+ 1);
				rename= TRUE ;
				break;
			case iop_submit:
				rm_ptr->f.fab$l_fop &= ~FAB$M_SCF & ~FAB$M_DLT ;
				submit= TRUE ;
				break;
			case iop_spool:
				rm_ptr->f.fab$l_fop &= ~FAB$M_SPL & ~FAB$M_DLT ;
				spool= TRUE ;
				break;
			case iop_destroy:
				rm_destroy = TRUE;
				break;
			case iop_nodestroy:
				rm_destroy = FALSE;
				break;
			case iop_rundown:
				rm_rundown = TRUE;
				break;
			default:
				break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
		}
	}
	if (promask != rm_ptr->promask)
	{	xab = cc$rms_xabpro;
		xab.xab$w_pro = rm_ptr->promask;
		rm_ptr->f.fab$l_xab = &xab;
	}
	nam = *(rm_ptr->f.fab$l_nam) ;
	status = sys$close(&rm_ptr->f);
	rm_ptr->f.fab$l_xab = 0;
	if (!rm_ptr->f.fab$w_ifi)
		iod->state = dev_closed;
	if (status != RMS$_NORMAL)
		rts_error(VARLSTCNT(2) status, rm_ptr->f.fab$l_stv);
	if (rename)
	{
		if ((status = sys$rename(&(rm_ptr->f), 0, 0, &fab)) != RMS$_NORMAL)
			rts_error(VARLSTCNT(2) status, rm_ptr->f.fab$l_stv);
	}
	if (spool)
	{
		status= iorm_jbc(&nam, pp, &pque, delete) ;
		if ((status & 1)==0)
		{
			rts_error(VARLSTCNT(1) status) ;
		}
	}
	if (submit)
	{
		status= iorm_jbc(&nam, pp, &bque, delete) ;
		if ((status & 1)==0)
		{
			rts_error(VARLSTCNT(1) status) ;
		}
	}
}
rm_ptr->r.rab$l_ctx = FAB$M_GET;
if (rm_destroy && !rm_rundown)
	remove_rms(iod);
return;
}
