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

#include "gtm_time.h"

#include <descrip.h>
#include <opcdef.h>
#include <ssdef.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "dcpsubs.h"
#include "decddp.h"
#include "five_2_ascii.h"
#include "util.h"

#define PUTINBUFFER(X)	(memcpy(bufptr, (X), SIZEOF(X)), bufptr += SIZEOF(X) - 1)
#define NEWLINE		(*bufptr++ = '\r' , *bufptr++ = '\n')
#define MAX_MSG_LEN	512

GBLREF bool                     dec_nofac;

void decddp_log_error(condition_code status, char *message_text, unsigned short *source_circuit, unsigned short *source_job)
{
	int4 			jobno, index;
	short			res_length;
	char			*bufptr;
	oper_msg_struct		oper;
	mstr			errmsg;
	$DESCRIPTOR(opmsg, "");
	error_def(ERR_SYSCALL);
	error_def(ERR_DDPLOGERR);

	bufptr = oper.text;
	PUTINBUFFER("GTM.DDP Server Status");
	if (NULL != source_circuit)
	{
		PUTINBUFFER("   Source circuit=");
		bufptr = five_2_ascii(source_circuit, bufptr);
	}
	if (NULL != source_job)
	{
		PUTINBUFFER("   Source job=");
		jobno = *source_job;
		bufptr += 5;
		for (index = 1; index <= 5; index++)
		{
			*(bufptr - index) = (jobno % 10) + '0';
			jobno /= 10;
		}
	}
	NEWLINE;
	if (NULL != message_text)
	{
		while (*bufptr++ = *message_text++)
			;
		bufptr--; /* remove the <NUL> terminator */
		NEWLINE;
	}
	if (0 != status)
	{
		assert(SIZEOF(oper.text) - MAX_MSG_LEN >= bufptr - oper.text);
		errmsg.addr = bufptr;
		errmsg.len = MAX_MSG_LEN;
		gtm_getmsg(status, &errmsg);
		bufptr += errmsg.len;
	}
	oper.req_code = OPC$_RQ_RQST;
	oper.target = OPC$M_NM_OPER1;
	opmsg.dsc$a_pointer = &oper;
	opmsg.dsc$w_length = SIZEOF(oper) - SIZEOF(oper.text) + (bufptr - oper.text);
	if (SS$_NORMAL != (status = sys$sndopr(&opmsg, 0))) /* to the operator log */
		dec_err(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("SYS$SNDOPR"), CALLFROM, status); /* record sndopr error in log */
	DDP_LOG_ERROR(bufptr - oper.text, oper.text); /* also to the error log */
	return;
}
