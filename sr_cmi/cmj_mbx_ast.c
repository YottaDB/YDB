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

#include <descrip.h>
#include <dvidef.h>
#include <iodef.h>
#include <msgdef.h>
#include <ssdef.h>
#include "cmihdr.h"
#include "cmidef.h"
#include "efn.h"

GBLREF struct dsc$descriptor_s cm_netname;
error_def(CMI_NETFAIL);
static void cmj_accept_ast(), cmj_reject_ast(), cmj_mbx_err();

void cmj_mbx_ast(struct NTD *tsk)
{
	struct CLB *cmu_makclb();
	struct dsc$descriptor_s ncb_desc;
	uint4 status, unit;
	cm_mbx *mp;
	struct CLB *lnk;
	bool newclb;
	void cmj_ast();

	status = 0;
	mp = tsk->mbx.dsc$a_pointer;
	assert(mp->netnum == 3 && mp->netnam[0] == 'N' && mp->netnam[1] == 'E' && mp->netnam[2] == 'T');
	switch(mp->msg)
	{
	case MSG$_CONNECT:
		lnk = cmj_unit2clb(tsk, mp->unit);
		if (lnk == 0)
		{
			newclb = TRUE;
			lnk = cmu_makclb();
			lnk->dch = tsk->dch;
			lnk->ntd = tsk;
		}else
		{
			newclb = FALSE;
			assert(lnk->sta == CM_CLB_IDLE);
		}
		ncb_desc.dsc$w_length = mp->len;
		ncb_desc.dsc$b_dtype = DSC$K_DTYPE_T;
		ncb_desc.dsc$b_class = DSC$K_CLASS_S;
		ncb_desc.dsc$a_pointer = mp->text;
		lnk->mbf = 0;
	/* the statement below and the 3 qio's emulate cmj_iostart which could be used if lnk->clb were a int4 */
		lnk->sta = CM_CLB_WRITE;
		if (tsk->crq == 0)
		{
			lnk->ast = cmj_reject_ast;
                        status = sys$qio(efn_ignore, lnk->dch, IO$_ACCESS | IO$M_ABORT,
				&lnk->ios, cmj_ast, lnk,
				lnk->mbf, &ncb_desc, 0, 0, 0, 0);
		} else
		{	if (tsk->acc && !(*tsk->acc)(lnk))
			{
				lnk->ast = cmj_reject_ast;
                                status = sys$qio(efn_ignore, lnk->dch, IO$_ACCESS | IO$M_ABORT,
					&lnk->ios, cmj_ast, lnk,
					lnk->mbf, &ncb_desc, 0, 0, 0, 0);
			}else
			{
				status = sys$assign(&cm_netname, &lnk->dch, 0, &tsk->mnm);
				if (status & 1)
				{
					status = lib$getdvi(&DVI$_UNIT, &lnk->dch, 0, &unit, 0, 0);
					if (status & 1)
					{
						lnk->mun = (unsigned short)unit;
						lnk->ast = cmj_accept_ast;
                                                status = sys$qio(efn_ignore, lnk->dch, IO$_ACCESS,
							&lnk->ios, cmj_ast, lnk,
							lnk->mbf, &ncb_desc, 0, 0, 0, 0);
					}
				}
			}
		}
		if ((status & 1) == 0)
		{
			if (newclb)
			{
				lib$free_vm(&SIZEOF(*lnk), &lnk, 0);
				lnk = 0;
			}
			cmj_mbx_err(status, tsk, lnk);
			break;
		}
		return;

	case MSG$_INTMSG:
		if (tsk->mbx_ast != 0)
		{	(*tsk->mbx_ast)(tsk);
			break;
		}
	/* CAUTION:  FALLTHROUGH */
	case MSG$_DISCON:
	case MSG$_ABORT:
	case MSG$_EXIT:
	case MSG$_PATHLOST:
	case MSG$_PROTOCOL:
	case MSG$_THIRDPARTY:
	case MSG$_TIMEOUT:
	case MSG$_NETSHUT:
	case MSG$_REJECT:
	case MSG$_CONFIRM:
		if (tsk->err)
		{
			lnk = cmj_unit2clb(tsk, mp->unit);
			(*tsk->err)(tsk, lnk, mp->msg);
		}else
			rts_error(CMI_NETFAIL,0,status);	/* condition handler would need to close the connection */
	/* CAUTION:  FALLTHROUGH */
	default:
		break;
	}

	status = cmj_mbx_read_start(tsk);
	if ((status & 1) == 0)
	{
		lnk = cmj_unit2clb(tsk, mp->unit);
		cmj_mbx_err(status, tsk, lnk);
	}
}

static void cmj_reject_ast(struct CLB *lnk)
{
	struct NTD	*tsk;
	uint4	status;

	tsk = lnk->ntd;
	status = lnk->ios.status;
	if ((status & 1) == 0)
	{
		if (cmj_unit2clb(tsk, lnk->mun) == 0)
		{
			lib$free_vm(&SIZEOF(*lnk), &lnk, 0);
			lnk = 0;
		}
		cmj_mbx_err(status, tsk, lnk);
	}

	status = cmj_mbx_read_start(tsk);
	if ((status & 1) == 0)
		cmj_mbx_err(status, tsk, lnk);
}

static void cmj_accept_ast(struct CLB *lnk)
{
	struct NTD	*tsk;
	uint4	status;

	tsk = lnk->ntd;
	status = lnk->ios.status;
	if ((status & 1) == 0)
	{
		if (cmj_unit2clb(tsk, lnk->mun) == 0)
		{
			lib$free_vm(&SIZEOF(*lnk), &lnk, 0);
			lnk = 0;
		}
		cmj_mbx_err(status, tsk, lnk);
	}else
	{
		insqhi(lnk, tsk);
		(*tsk->crq)(lnk);
	}

	status = cmj_mbx_read_start(tsk);
	if ((status & 1) == 0)
		cmj_mbx_err(status, tsk, lnk);
}

static void cmj_mbx_err(uint4 status, struct NTD *tsk, struct CLB *lnk)
{
        unsigned int  msg;

	if (tsk->err)
	{
		switch (status)
		{
		case SS$_LINKABORT:
			msg = MSG$_ABORT;
			break;
		case SS$_LINKDISCON:
			msg = MSG$_DISCON;
			break;
		case SS$_LINKEXIT:
			msg = MSG$_EXIT;
			break;
		case SS$_PATHLOST:
			msg = MSG$_PATHLOST;
			break;
		case SS$_PROTOCOL:
			msg = MSG$_PROTOCOL;
			break;
		case SS$_THIRDPARTY:
			msg = MSG$_THIRDPARTY;
			break;
		case SS$_CONNECFAIL:
		case SS$_TIMEOUT:
			msg= MSG$_TIMEOUT;
			break;
		case SS$_REJECT:
			msg = MSG$_REJECT;
			break;
		default:
		case SS$_DEVALLOC:
		case SS$_IVDEVNAM:
			assert(FALSE);
		/* CAUTION:  FALLTHROUGH */
		case SS$_NOSUCHNODE:
		case SS$_UNREACHABLE:
			msg = MSG$_NODEINACC;
			break;
		}
		(*tsk->err)(tsk, lnk, msg);
	}else
		rts_error(CMI_NETFAIL,0,status);	/* condition handler would need to close the connection */
}
