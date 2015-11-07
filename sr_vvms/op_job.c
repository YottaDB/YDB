/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <stdarg.h>

#include "ast.h"
#include "efn.h"
#include "job.h"
#include "op.h"
#include "io.h"
#include "gt_timer.h"
#include "iotimer.h"
#include "outofband.h"
#include "rel_quant.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

LITDEF mstr		define_gtm$job$_	= {16, "DEFINE GTM$JOB$ "};
LITDEF mstr		set_default_		= {12, "SET DEFAULT "};
LITDEF mstr		atsign			= {1,  "@"};
LITDEF mstr		run__nodebug_		= {13, "RUN /NODEBUG "};
LITDEF mstr		set_noverify		= {12, "SET NOVERIFY"};

GBLDEF	bool		ojtimeout	= TRUE;
GBLDEF	short		ojpchan		= 0;
GBLDEF	short		ojcchan		= 0;
GBLDEF	uint4		ojcpid		= 0;
GBLDEF	short		ojastq;

GBLREF	short	astq_dyn_avail;
GBLREF	mval	dollar_job;
GBLREF	uint4	dollar_trestart;
GBLREF	uint4	dollar_zjob;
GBLREF	int4	outofband;

error_def(ERR_ACK);
error_def(ERR_ENQ);
error_def(ERR_INSFFBCNT);
error_def(ERR_JOBARGMISSING);
error_def(ERR_JOBFAIL);

readonly $DESCRIPTOR	(loginout, "SYS$SYSTEM:LOGINOUT.EXE");

#define JOBTIMESTR "JOB time too long"

int op_job(mval *label, ...)
{
	va_list			var, save;
	bool			defprcnam, timed;
	/* The max possible value for combuff is 268, 256 bytes for DEFAULT + 12 bytes for set_default_ */
	char			combuf[268];
	int4			argcnt, i, offset, timeout;
	unsigned int		ast_stat;
	mstr			command;
	mval			*parameters, *routine;
	/* Parameters ... */
	mval			*inp;
	mstr			deffs, error, gbldir, image, input, output, startup;
	struct dsc$descriptor_s	logfile, prcnam;
	int4			baspri, stsflg;
	quadword		schedule;
	/* Mailboxes ... */
	int4			cmaxmsg, punit;
	char			cmbxnam[MAX_MBXNAM_LEN];
	$DESCRIPTOR(cmbx, cmbxnam);
	/* $CREPRC ... */
	unsigned int		status;
	/* Messages */
	mstr			dummstr, isddsc;
	isd_type		isd;
	mbx_iosb		iosb;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Initializations ... */
	assert(TRUE == ojtimeout);
	assert(0 == ojpchan);
	assert(0 == ojcchan);
	assert(0 == ojcpid);
	assert(0 == ojastq);
	VAR_START(var, label);
	va_count(argcnt);
	assert(argcnt >= 5);
	offset = va_arg(var, int4);
	routine = va_arg(var, mval *);
	parameters = va_arg(var, mval *);
	timeout = va_arg(var, int4);
	argcnt -= 5;
	VAR_COPY(save, var);
	/* initialize $zjob = 0, in case JOB fails */
	dollar_zjob = 0;
	MV_FORCE_DEFINED(label);
	MV_FORCE_DEFINED(routine);
	MV_FORCE_DEFINED(parameters);
	cmaxmsg = SIZEOF(isd_type);
	for (i = argcnt;  i;  i--)
	{
		inp = va_arg(var, mval *);
		if (!inp->mvtype)
		{
			if (1 == i)
			{	/* if it's really empty let it work like it did before, even though it's not right */
				argcnt--;
				break;
			}
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_JOBARGMISSING, 1, argcnt - i);
		}
		MV_FORCE_STR(inp);
		if (inp->str.len > cmaxmsg)
			cmaxmsg = inp->str.len;
	}
	va_end(var);		/* need before used as destination in va_copy */
	command.addr = combuf;
	ojparams(parameters->str.addr, routine, &defprcnam, &cmaxmsg,
			&image, &input, &output, &error, &prcnam, &baspri,
			&stsflg, &gbldir, &startup, &logfile, &deffs,
			&schedule);
	flush_pio();
	if (!ojchkbytcnt(cmaxmsg))
	{
		va_end(save);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INSFFBCNT);
	}
	if (timeout < 0)
		timeout = 0;
	else if (TREF(tpnotacidtime) < timeout)
		TPNOTACID_CHECK(JOBTIMESTR);
	timed = (NO_M_TIMEOUT != timeout);
	if (timed)
		ojtmrinit(&timeout);
	if (!ojcrembxs(&punit, &cmbx, cmaxmsg, timed))
	{
		assert(timed);
		assert(ojtimeout);
		if (0 != ojpchan)
		{
			status = sys$dassgn(ojpchan);
			if (!(status & 1))
			{
				ojerrcleanup();
				va_end(save);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
			ojpchan = 0;
		}
		if (0 != ojcchan)
		{
			status = sys$dassgn(ojcchan);
			if (!(status & 1))
			{
				ojerrcleanup();
				va_end(save);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
			ojcchan = 0;
		}
		return (FALSE);
	}
	/* Allocate AST for mailbox reads */
	ast_stat = sys$setast(DISABLE);
	while (astq_dyn_avail < 1)
	{
		ENABLE_AST;
		rel_quant();
		DISABLE_AST;
	}
	ojastq++;
	--astq_dyn_avail;
	if (SS$_WASSET == ast_stat)
		sys$setast(ENABLE);
	ojsetattn(ERR_ENQ);
	do
	{
		status = sys$creprc(&ojcpid, &loginout, &cmbx, &logfile, 0,
				0, 0, &prcnam, baspri, 0, punit, stsflg);
		if (outofband)
		{
			ojcleanup();
			va_end(save);
			outofband_action(FALSE);
		}
		switch (status)
		{
		case SS$_NORMAL:
			break;
		case SS$_DUPLNAM:
			if (defprcnam)
			{
				ojdefprcnam(&prcnam);
				continue;
			}
		case SS$_NOSLOT:
		case SS$_INSSWAPSPACE:
		case SS$_EXPRCLM:
			hiber_start(1000);
			if (timed && ojtimeout)
			{
				status = sys$dassgn(ojpchan);
				if (!(status & 1))
				{
					ojerrcleanup();
					va_end(save);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
				ojpchan = 0;
				status = sys$dassgn(ojcchan);
				if (!(status & 1))
				{
					ojerrcleanup();
					va_end(save);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
				}
				ojcchan = 0;
				ojcpid = 0;
				--ojastq;
				astq_dyn_avail++;
				va_end(save);
				return (FALSE);
			}
			break;
		default:
			ojerrcleanup();
			va_end(save);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_JOBFAIL, 0, status);
			break;
		}
	} while (SS$_NORMAL != status);
	ojmbxio(IO$_WRITEVBLK, ojcchan, &set_noverify, &iosb, TRUE);
	memcpy(combuf, define_gtm$job$_.addr, define_gtm$job$_.len);
	assert(MV_IS_STRING(&dollar_job));
	memcpy(&combuf[define_gtm$job$_.len], dollar_job.str.addr, dollar_job.str.len);
	command.len = define_gtm$job$_.len + dollar_job.str.len;
	ojmbxio(IO$_WRITEVBLK, ojcchan, &command, &iosb, TRUE);
	memcpy(combuf, set_default_.addr, set_default_.len);
	memcpy(&combuf[set_default_.len], deffs.addr, deffs.len);
	command.len = set_default_.len + deffs.len;
	ojmbxio(IO$_WRITEVBLK, ojcchan, &command, &iosb, TRUE);
	if (0 != startup.len)
	{
		memcpy(combuf, atsign.addr, atsign.len);
		memcpy(&combuf[atsign.len], startup.addr, startup.len);
		command.len = atsign.len + startup.len;
		ojmbxio(IO$_WRITEVBLK, ojcchan, &command, &iosb, TRUE);
	}
	memcpy(combuf, run__nodebug_.addr, run__nodebug_.len);
	memcpy(&combuf[run__nodebug_.len], image.addr, image.len);
	command.len = run__nodebug_.len + image.len;
	ojmbxio(IO$_WRITEVBLK, ojcchan, &command, &iosb, TRUE);
	status = sys$waitfr(efn_op_job);
	if (!(status & 1))
	{
		ojerrcleanup();
		va_end(save);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
	/* Assertion: successful ENQ */
	ojsetattn(ERR_ACK);
	ojmbxio(IO$_WRITEVBLK, ojcchan, &input, &iosb, TRUE);
	ojmbxio(IO$_WRITEVBLK, ojcchan, &output, &iosb, TRUE);
	ojmbxio(IO$_WRITEVBLK, ojcchan, &error, &iosb, TRUE);
	ojmbxio(IO$_WRITEVBLK, ojcchan, &gbldir, &iosb, TRUE);
	isddsc.len = SIZEOF(isd);
	isddsc.addr = &isd;
	assert(label->str.len <= MAX_MIDENT_LEN);
	memset(&isd.label, 0, SIZEOF(isd.label));
	memcpy(&isd.label.c[0], label->str.addr, label->str.len);
	isd.offset = offset;
	assert(routine->str.len <= MAX_MIDENT_LEN);
	memset(&isd.routine, 0, SIZEOF(isd.routine));
	memcpy(&isd.routine.c[0], routine->str.addr, routine->str.len);
	isd.schedule.lo = schedule.lo;
	isd.schedule.hi = schedule.hi;
	ojmbxio(IO$_WRITEVBLK, ojcchan, &isddsc, &iosb, TRUE);
	isddsc.len = SIZEOF(argcnt);
	isddsc.addr = &argcnt;
	ojmbxio(IO$_WRITEVBLK, ojcchan, &isddsc, &iosb, TRUE);
	if (argcnt)
	{
		while (argcnt--)
		{
			inp = va_arg(save, mval *);
			MV_FORCE_DEFINED(inp);		/* In case undefined mval in NOUNDEF mode */
			assert(inp->mvtype & MV_STR);
			isddsc = inp->str;
			ojmbxio(IO$_WRITEVBLK, ojcchan, &isddsc, &iosb, TRUE);
		}
	}
	va_end(save);
	status = sys$waitfr(efn_op_job);
	if (!(status & 1))
	{
		ojerrcleanup();
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
	/* Assertion: successful ACK */
	--ojastq;
	astq_dyn_avail++;
	status = sys$dassgn(ojpchan);
	if (!(status & 1))
	{
		ojerrcleanup();
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
	ojpchan = 0;
	dummstr.len = 0;
	dummstr.addr = 0;
	ojmbxio(IO$_WRITEOF, ojcchan, &dummstr, &iosb, TRUE);
	status = sys$dassgn(ojcchan);
	if (!(status & 1))
	{
		ojerrcleanup();
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	}
	ojcchan = 0;
	if (timed)
	{
		ast_stat = sys$setast(DISABLE);
		if (!ojtimeout)
		{
			assert(1 == ojastq);
			sys$cantim(&(ojtimeout), 0);
			--ojastq;
			astq_dyn_avail++;
			ojtimeout = TRUE;
		}
		if (SS$_WASSET == ast_stat)
			sys$setast(ENABLE);
	}
	assert(TRUE == ojtimeout);
	assert(0 != ojcpid);
	dollar_zjob = ojcpid;
	ojcpid = 0;
	return TRUE;
}
