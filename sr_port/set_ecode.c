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
#include "mvalconv.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "merrors_ansi.h"

GBLREF ecode_list	*dollar_ecode_list;

void set_ecode(int errnum)
{
	mval		valm, valz;
	err_ctl		*ectl;
	char		m_num_buff[MAX_NUM_SIZE], z_num_buff[MAX_NUM_SIZE];
	char		*ecode_ptr;
	int		ansi_error;
	int		severity;
	ecode_list	*ecode_next;
	error_def(ERR_SETECODE);

	/* If this routine was called with error code SETECODE,
	 * an end-user just put a correct value into $ECODE,
	 * and it shouldn't be replaced by this routine.
	 */
	if (ERR_SETECODE == errnum)
		return;
	/* When the value of $ECODE is non-empty, error trapping
	* will be invoked. When the severity level does not warrant
	* error trapping, no value should be copied into $ECODE
	*/
	severity = errnum & SEV_MSK;
	if ((INFO == severity) || (SUCCESS == severity))
		return;
	/* The malloc()s in this function are freed when $ECODE is set to empty in op_svput.c */
	ecode_next = malloc(sizeof(ecode_list));
	ecode_next->previous = dollar_ecode_list;
	dollar_ecode_list = ecode_next;
	ecode_next->level = dollar_zlevel();
	/* Set $ECODE
	 * If the error has an ANSI standard code,
	 * return ,Mnnn, (nnn is ANSI code).
	 * Always return ,Zxxx, (xxx is GT.M code)
	 * Note that the value of $ECODE must start and end with a comma
	 */
	valz.str.addr = z_num_buff;
	MV_FORCE_MVAL(&valz, errnum);
	n2s(&valz);
	ecode_next->str.len = sizeof(",Z,") - 1 + valz.str.len;
	ansi_error = 0;
	if (ectl = err_check(errnum))
	{
		ansi_error = ((errnum & FACMASK(ectl->facnum)) &&
			(MSGMASK(errnum, ectl->facnum) <= ectl->msg_cnt))
		? error_ansi[MSGMASK(errnum, ectl->facnum) - 1]
		: 0;
		if (ansi_error > 0)
		{
			valm.str.addr = m_num_buff;
			MV_FORCE_MVAL(&valm, ansi_error);
			n2s(&valm);
			ecode_next->str.len += (sizeof("M,") - 1 + valm.str.len);
		}
	}
	ecode_ptr = ecode_next->str.addr = malloc(ecode_next->str.len);
	*ecode_ptr++ = ',';
	if (ansi_error > 0)
	{
		*ecode_ptr++ = 'M';
		memcpy(ecode_ptr, valm.str.addr, valm.str.len);
		ecode_ptr += valm.str.len;
		*ecode_ptr++ = ',';
	}
	*ecode_ptr++ = 'Z';
	memcpy(ecode_ptr, valz.str.addr, valz.str.len);
	ecode_ptr += valz.str.len;
	*ecode_ptr++ = ',';
}
