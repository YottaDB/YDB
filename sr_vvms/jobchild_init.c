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
#include <ssdef.h>
#include <descrip.h>
#include <iodef.h>
#include <lnmdef.h>
#include <dvidef.h>
#include <jpidef.h>
#include <stsdef.h>
#include "vmsdtype.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "stringpool.h"
#include "job.h"
#include "efn.h"
#include "op.h"
#include "zbreak.h"
#include "jobchild_init.h"
#include "cmd_qlf.h"
#include "lv_val.h"	/* needed for "callg.h" */
#include "callg.h"

#define	FATAL(error)	(error & STS$M_COND_ID | STS$K_SEVERE)

GBLDEF unsigned short	pchan;

GBLREF spdesc		stringpool;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_ACK);
error_def(ERR_ENQ);
error_def(ERR_FMLLSTMISSING);
error_def(ERR_JOBFAIL);
error_def(ERR_JOBLABOFF);
error_def(ERR_STRINGOFLOW);

void jobchild_init(unsigned char *base_addr_ptr)
{
	unsigned	status;
	mbx_iosb	iosb;
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	} item_list;
	short		dummy;
	$DESCRIPTOR	(tabnam, "LNM$PROCESS");
	$DESCRIPTOR	(lognam, "GTM$JOB$");
	mval		ppid;
	char		ppidstr[MAX_PIDSTR_LEN];
	short		ppidstrlen;
	short		cchan;
	int4		punit, cunit;
	char		pmbxnam[32];
	$DESCRIPTOR	(pmbx, &pmbxnam[0]);

/* Parameters ... */
	mstr		input, output, error, gbldir;

/* Messages */
	int4		cmaxmsg;
	mstr		stsdsc;
	pmsg_type	stsmsg;
	mstr		isddsc;
	isd_type	isd;
	mstr		argstr;
	int4		argcnt, i;
	mval		*arglst;
	mval		rt;

	struct {
		int4	callargs;
		int4	truth;
		int4	retval;
		int4	mask;
		int4	argcnt;
		mval	*argval[1];
		} *gcall_arg;
/* Transfer data */
	rhdtyp		*rt_hdr;
	mstr		label, rtn;
	int4		*lp;
	DCL_THREADGBL_ACCESS;

	LITDEF $DESCRIPTOR		(sys$input,  "SYS$INPUT");
	LITDEF $DESCRIPTOR		(sys$output, "SYS$OUTPUT");
	LITDEF $DESCRIPTOR		(sys$error,  "SYS$ERROR");
	LITDEF $DESCRIPTOR		(gtm$gbldir, "GTM$GBLDIR");

	SETUP_THREADGBL_ACCESS;
	item_list.item[0].buffer_length		= MAX_PIDSTR_LEN;
	item_list.item[0].item_code		= LNM$_STRING;
	item_list.item[0].buffer_address	= &ppidstr[0];
	item_list.item[0].return_length_address	= &ppidstrlen;
	item_list.terminator			= 0;
	status = sys$trnlnm(0, &tabnam, &lognam, 0, &item_list);
	if (SS$_NOLOGNAM == status)
	{
		new_stack_frame(base_addr_ptr, base_addr_ptr + SIZEOF(rhdtyp), base_addr_ptr + SIZEOF(rhdtyp));
		return;
	}
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) FATAL(status));
	if (MAX_PIDSTR_LEN < ppidstrlen)
		rts_error(VARLSTCNT(1) FATAL(ERR_JOBFAIL));
	ppid.mvtype = MV_STR;
	ppid.str.len = ppidstrlen;
	ppid.str.addr = &ppidstr[0];
	s2n(&ppid);
	status = sys$assign(&sys$input, &cchan, 0, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) FATAL(status));
	item_list.item[0].buffer_length		= SIZEOF(cmaxmsg);
	item_list.item[0].item_code		= DVI$_DEVBUFSIZ;
	item_list.item[0].buffer_address	= &cmaxmsg;
	item_list.item[0].return_length_address	= &dummy;
	item_list.terminator			= 0;
	status = sys$getdvi(efn_immed_wait, cchan, 0, &item_list, &iosb, 0, 0, 0);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) FATAL(status));
	sys$synch(efn_immed_wait, &iosb);
	if (SS$_NORMAL != iosb.status)
		rts_error(VARLSTCNT(1) FATAL(iosb.status));
	item_list.item[0].buffer_length		= SIZEOF(punit);
	item_list.item[0].item_code		= JPI$_TMBU;
	item_list.item[0].buffer_address	= &punit;
	item_list.item[0].return_length_address	= &dummy;
	item_list.terminator			= 0;
	status = sys$getjpi(efn_immed_wait, 0, 0, &item_list, &iosb, 0, 0);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) FATAL(status));
	sys$synch(efn_immed_wait, &iosb);
	if (SS$_NORMAL != iosb.status)
		rts_error(VARLSTCNT(1) FATAL(iosb.status));
	pmbx.dsc$w_length = ojunit_to_mba(&pmbxnam[0], punit);
	status = sys$assign(&pmbx, &pchan, 0, 0);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) FATAL(status));
	lib$establish(ojch);	/* now we have the mailbox, use the condition handler to send status and exit */
	stsmsg.unused = 0;
	stsmsg.finalsts = ERR_ENQ;
	stsdsc.addr = &stsmsg;
	stsdsc.len = SIZEOF(stsmsg);
	ojmbxio(IO$_WRITEVBLK, pchan, &stsdsc, &iosb, TRUE);
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	input.len = cmaxmsg;
	input.addr = stringpool.free;
	ojmbxio(IO$_READVBLK, cchan, &input, &iosb, FALSE);
	input.len = iosb.byte_count;
	stringpool.free += input.len;
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	output.len = cmaxmsg;
	output.addr = stringpool.free;
	ojmbxio(IO$_READVBLK, cchan, &output, &iosb, FALSE);
	output.len = iosb.byte_count;
	stringpool.free += output.len;
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	error.len = cmaxmsg;
	error.addr = stringpool.free;
	ojmbxio(IO$_READVBLK, cchan, &error, &iosb, FALSE);
	error.len = iosb.byte_count;
	stringpool.free += error.len;
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	gbldir.len = cmaxmsg;
	gbldir.addr = stringpool.free;
	ojmbxio(IO$_READVBLK, cchan, &gbldir, &iosb, FALSE);
	gbldir.len = iosb.byte_count;
	stringpool.free += gbldir.len;
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	isddsc.len = SIZEOF(isd);
	isddsc.addr = &isd;
	ojmbxio(IO$_READVBLK, cchan, &isddsc, &iosb, FALSE);
	assert(SIZEOF(isd) == iosb.byte_count);
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
		rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
	argstr.len = SIZEOF(argcnt);
	argstr.addr = &argcnt;
	ojmbxio(IO$_READVBLK, cchan, &argstr, &iosb, FALSE);
	assert(SIZEOF(argcnt) == iosb.byte_count);
	assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
	if (argcnt)
	{	/* process incoming parameters list */
		arglst = malloc(argcnt * SIZEOF(mval));
		gcall_arg = malloc(SIZEOF(*gcall_arg) + (argcnt - 1) * SIZEOF(mval *));
		gcall_arg->callargs = argcnt + 4;
		gcall_arg->truth = 1;
		gcall_arg->retval = 0;
		gcall_arg->mask = 0;
		gcall_arg->argcnt = argcnt;
		for (i = 0; i < argcnt; i++)
		{
			if (!IS_STP_SPACE_AVAILABLE(cmaxmsg))
				rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
			argstr.len = cmaxmsg;
			argstr.addr = stringpool.free;
			ojmbxio(IO$_READVBLK, cchan, &argstr, &iosb, FALSE);
			assert(iosb.pid == (int4)MV_FORCE_INTD(&ppid));
			argstr.len = iosb.byte_count;
			stringpool.free += argstr.len;
			arglst[i].mvtype = MV_STR;
			arglst[i].str = argstr;
			gcall_arg->argval[i] = &arglst[i];
		}
	}
	status = sys$dassgn(cchan);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	stsmsg.unused = 0;
	stsdsc.addr = &stsmsg;
	stsdsc.len = SIZEOF(stsmsg);
	rtn.addr = &isd.routine.c[0];
	rtn.len = mid_len(&isd.routine);
	if (0 == (rt_hdr = find_rtn_hdr(&rtn)))
	{
		rt.mvtype = MV_STR;
		rt.str = rtn;
		op_zlink(&rt, 0);
		if (0 == (rt_hdr = find_rtn_hdr(&rtn)))
			GTMASSERT;
	}
	base_addr_ptr = (char *)rt_hdr;
	label.addr = &isd.label.c[0];
	label.len = mid_len(&isd.label);
	lp = NULL;
	if ((rt_hdr->compiler_qlf & CQ_LINE_ENTRY) || (0 == isd.offset))
		/* label offset with routine compiled with NOLINE_ENTRY should cause error */
		lp = find_line_addr(rt_hdr, &label, isd.offset, NULL);
	if (!lp)
		rts_error(VARLSTCNT(1) ERR_JOBLABOFF);
	if (argcnt && !(TREF(lab_proxy)).has_parms)		/* Label has no formallist, but is passed an actuallist. */
		rts_error(VARLSTCNT(1) ERR_FMLLSTMISSING);
	item_list.item[0].item_code		= LNM$_STRING;
	item_list.item[0].return_length_address	= &dummy;
	item_list.terminator			= 0;
	item_list.item[0].buffer_length		= input.len;
	item_list.item[0].buffer_address	= input.addr;
	status = sys$crelnm(0, &tabnam, &sys$input, 0, &item_list);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	item_list.item[0].buffer_length		= output.len;
	item_list.item[0].buffer_address	= output.addr;
	status = sys$crelnm(0, &tabnam, &sys$output, 0, &item_list);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	item_list.item[0].buffer_length		= error.len;
	item_list.item[0].buffer_address	= error.addr;
	status = sys$crelnm(0, &tabnam, &sys$error, 0, &item_list);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	if (0 != gbldir.len)
	{
		item_list.item[0].buffer_length		= gbldir.len;
		item_list.item[0].buffer_address	= gbldir.addr;
		status = sys$crelnm(0, &tabnam, &gtm$gbldir, 0, &item_list);
		if (!(status & 1))
			rts_error(VARLSTCNT(1) status);
	}
	stsmsg.finalsts = ERR_ACK;
	ojmbxio(IO$_WRITEVBLK, pchan, &stsdsc, &iosb, TRUE);
	status = sys$dassgn(pchan);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	if ((0 != isd.schedule.lo) || (0 != isd.schedule.hi))
	{
		status = sys$schdwk(0, 0, &isd.schedule, 0);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
		sys$hiber();
	}
	new_stack_frame(base_addr_ptr, LINE_NUMBER_ADDR(rt_hdr, lp), LINE_NUMBER_ADDR(rt_hdr, lp));
	if (argcnt)
		callg(push_parm, (gparam_list *)gcall_arg);
	lib$revert();
}
