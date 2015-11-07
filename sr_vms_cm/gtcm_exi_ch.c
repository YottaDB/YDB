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

#include <descrip.h>
#include <rms.h>
#include <opcdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "error.h"

typedef struct {
	char			req_code;
	char			target[3];
	int4			mess_code;
	char			text[255];
}oper_msg_struct;

#define MESSAGE_HDR_LEN 32	/* length of rundownerr string */

GBLREF bool		gtcm_errfile;
GBLREF struct FAB	gtcm_errfab;
GBLREF struct RAB	gtcm_errrab;
GBLREF int4		gtcm_exi_condition;
GBLREF int4		process_id;

CONDITION_HANDLER(gtcm_exi_ch)
{
	static readonly char	filename[] = "SYS$MANAGER:CMEXIT.LOG";
	static readonly char	rundownerr[] = "GTCM TERMINATION RUNDOWN ERROR :";
	char			buff[512], buff2[512], errout[259], errbuff[300], outadr[4];
	unsigned short		faolen, msg_len, severe_len;
	uint4			*argptr, *a_ptr, flags = 0, stat;
	oper_msg_struct		oper_msg;

	$DESCRIPTOR(msg, buff);
	$DESCRIPTOR(msg2, buff2);
	$DESCRIPTOR(err1, errout);
	$DESCRIPTOR(out, "");
	$DESCRIPTOR(out1, "");

	if (!gtcm_errfile)				/* open error logging file */
	{	gtcm_errfab = cc$rms_fab;
		gtcm_errrab = cc$rms_rab;
		gtcm_errrab.rab$l_fab = &gtcm_errfab;
		gtcm_errrab.rab$l_rop = RAB$M_WBH;
		gtcm_errfab.fab$b_fns = SIZEOF(filename);
		gtcm_errfab.fab$l_fna = filename;
		gtcm_errfab.fab$b_shr = FAB$M_SHRGET | FAB$M_UPI;
		gtcm_errfab.fab$l_fop = FAB$M_MXV | FAB$M_CBT;
		gtcm_errfab.fab$b_fac = FAB$M_PUT;
		gtcm_errfab.fab$b_rat = FAB$M_CR;
		stat = sys$create(&gtcm_errfab);
		if (stat & 1)
		{	stat = sys$connect(&gtcm_errrab);
			if (RMS$_NORMAL == stat)
				gtcm_errfile = TRUE;
		}
	}
	argptr = &SIGNAL;
	sys$getmsg(*argptr, &msg_len, &msg, flags, outadr);
	msg.dsc$w_length = msg_len;
	out.dsc$w_length = SIZEOF(oper_msg.text) - MESSAGE_HDR_LEN;
	out.dsc$a_pointer = &oper_msg.text[MESSAGE_HDR_LEN];
	a_ptr = argptr + 1;
	faolen = 0;
	if (sig->chf$l_sig_args > 3)
	{	switch(*a_ptr)
		{
		case 0:	sys$fao(&msg, &faolen, &out);
			break;
		case 1:	sys$fao(&msg, &faolen, &out, *(a_ptr+1));
			break;
		case 2:	sys$fao(&msg, &faolen, &out, *(a_ptr+1), *(a_ptr+2));
			break;
		case 3:	sys$fao(&msg, &faolen, &out, *(a_ptr+1), *(a_ptr+2), *(a_ptr+3));
			break;
		case 4:	sys$fao(&msg, &faolen, &out, *(a_ptr+1), *(a_ptr+2), *(a_ptr+3), *(a_ptr+4));
			break;
		case 5:	sys$fao(&msg, &faolen, &out, *(a_ptr+1), *(a_ptr+2), *(a_ptr+3), *(a_ptr+4), *(a_ptr+5));
			break;
		case 6:	sys$fao(&msg, &faolen, &out, *(a_ptr+1), *(a_ptr+2), *(a_ptr+3), *(a_ptr+4), *(a_ptr+5), *(a_ptr+6));
			break;
		}
	} else
		sys$fao(&msg, &faolen, &out);
	severe_len = faolen;
	if (gtcm_errfile)
	{	memcpy(errout, "!%D ", 4);
		memcpy(errout + 4, &oper_msg.text[MESSAGE_HDR_LEN], faolen);
		err1.dsc$w_length =  faolen + 4;
		out1.dsc$w_length = SIZEOF(errbuff);
		out1.dsc$a_pointer = errbuff;
		sys$fao(&err1, &faolen, &out1, 0);
		gtcm_errrab.rab$w_rsz = faolen;
		gtcm_errrab.rab$l_rbf = errbuff;
		sys$put(&gtcm_errrab);
		sys$flush(&gtcm_errrab);
	}
	memcpy(oper_msg.text, rundownerr, MESSAGE_HDR_LEN);
	msg2.dsc$a_pointer = &oper_msg;
	msg2.dsc$w_length = severe_len + 8 + MESSAGE_HDR_LEN;
	oper_msg.req_code = OPC$_RQ_RQST;
	*oper_msg.target = OPC$M_NM_CENTRL | OPC$M_NM_DEVICE | OPC$M_NM_DISKS;
	sys$sndopr(&msg2, 0);
	EXIT(gtcm_exi_condition);
	NEXTCH;
}
