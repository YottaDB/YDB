/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include "gdsroot.h"
#include "ccp.h"
#include <descrip.h>
#include <lnmdef.h>
#include <opcdef.h>

GBLREF	uint4	process_id;
GBLREF	bool		ccp_dump_on;

error_def(ERR_CCPSIGCONT);

static	unsigned char	pid_msg[] = "GT.CX_CONTROL--PID: ",
			pc_msg[] = "PC: ";

static	$DESCRIPTOR	(lnm$dcl_logical, "LNM$DCL_LOGICAL");
static	$DESCRIPTOR	(gtm$deadlock, "GTM$DEADLOCK");

static	int		attr = LNM$M_CASE_BLIND;


CONDITION_HANDLER(ccp_ch)
{
	struct
	{
		char	type;
		int	target : 24;
		int4	rqstid;
		char	text[255];
	}			oper_msg;

	struct dsc$descriptor_s	oper_msg_dsc, sig_msg_dsc, fao_out_dsc;
	int4			*pc, count;
	uint4		*c, status;
	unsigned short		sig_msg_len, fao_len, oper_text_len;
	char			*text_ptr, sig_msg_buffer[255];


	c = &SIGNAL;

	/* Don't report CANCEL, and don't report DEADLOCK unless logical name GTM$DEADLOCK is defined */
	if (SIGNAL == ERR_CCPSIGCONT  &&  sig->chf$l_sig_args > 2  &&
	    (*(c + 3) == SS$_CANCEL  ||
	     *(c + 3) == SS$_DEADLOCK  &&  sys$trnlnm(&attr, &lnm$dcl_logical, &gtm$deadlock, NULL, NULL) != SS$_NORMAL))
		CONTINUE;

	if (ccp_dump_on)
		ccp_dump();


	/* Initialize constant fields */

	oper_msg_dsc.dsc$b_dtype = fao_out_dsc.dsc$b_dtype
				 = sig_msg_dsc.dsc$b_dtype
				 = DSC$K_DTYPE_T;

	oper_msg_dsc.dsc$b_class = fao_out_dsc.dsc$b_class
				 = sig_msg_dsc.dsc$b_class
				 = DSC$K_CLASS_S;

	oper_msg_dsc.dsc$a_pointer = &oper_msg;
	sig_msg_dsc.dsc$a_pointer = sig_msg_buffer;

	oper_msg.type = OPC$_RQ_RQST;
	oper_msg.target = OPC$M_NM_CLUSTER;	/***** This should be setable by CCE *****/
	oper_msg.rqstid = 0;


	/* Move PID text into message */
	text_ptr = oper_msg.text;
	memcpy(text_ptr, pid_msg, sizeof pid_msg - 1);
	text_ptr += sizeof pid_msg - 1;
	text_ptr += ojhex_to_str(process_id, text_ptr);
	*text_ptr++ = ' ';

	if (SIGNAL != ERR_CCPSIGCONT)
	{
		pc = (int4 *)sig + sig->chf$l_sig_args - 1;
		memcpy(text_ptr, pc_msg, sizeof pc_msg - 1);
		text_ptr += sizeof pc_msg - 1;
		text_ptr += ojhex_to_str(*pc, text_ptr);
		*text_ptr++ = ' ';
	}

	fao_out_dsc.dsc$w_length = sizeof oper_msg.text - (text_ptr - oper_msg.text);
	count = sig->chf$l_sig_args - 2;

	for (;;)
	{
		sig_msg_dsc.dsc$w_length = sizeof sig_msg_buffer;
		sys$getmsg(*c++, &sig_msg_len, &sig_msg_dsc, 0, NULL);
		sig_msg_dsc.dsc$w_length = sig_msg_len;

		fao_out_dsc.dsc$a_pointer = text_ptr;
		/* Allow max of 4 fao args, will ignore if not used */
		sys$fao(&sig_msg_dsc, &fao_len, &fao_out_dsc, *(c+1), *(c+2), *(c+3), *(c+4));
		text_ptr += fao_len;

		if (--count != 0)
			if (fao_len == sig_msg_len)
			{
				if (*c == 0)
				{
					c++;
					count--;
				}
			}
			else
			{
				count -= *c + 1;
				c += *c + 1;
			}

		if (count == 0)
			break;

		*text_ptr++ = ' ';
		fao_out_dsc.dsc$w_length -= fao_len + 1;
	}

	oper_msg_dsc.dsc$w_length = text_ptr - (char *)&oper_msg;
	sys$sndopr(&oper_msg_dsc, 0);

	/* Drop active entry, if any, and continue to operate */
	if (SIGNAL == ERR_CCPSIGCONT)
		CONTINUE;

	NEXTCH;
}
