/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "error_trap.h"
#include "merrors_ansi.h"

/* ECODE_MAX_LEN is the maximum length of the string representation of "errnum"'s ECODE.
 * This is arrived at as follows : ",M<number>,Z<number>,". Each number can be at most MAX_NUM_SIZE.
 * The three ","s and the letters "M" and "Z" add up to 5 characters.
 * In addition we give a buffer for overflow in the production version (just in case).
 */
#define	BUFFER_FOR_OVERFLOW	15	/* give some buffer in case overflow happens in PRO */
#define	ECODE_MAX_LEN			((2 * MAX_DIGITS_IN_INT) + STR_LIT_LEN(",M,Z,"))
#define	ECODE_MAX_LEN_WITH_BUFFER	((ECODE_MAX_LEN) + (BUFFER_FOR_OVERFLOW))

error_def(ERR_SETECODE);

void ecode_set(int errnum)
{
	mval		tmpmval;
	const err_ctl	*ectl;
	mstr		ecode_mstr;
	char		ecode_buff[ECODE_MAX_LEN_WITH_BUFFER];
	char		*ecode_ptr;
	int		ansi_error;
	int		severity;

	/* If this routine was called with error code SETECODE,
	 * an end-user just put a correct value into $ECODE,
	 * and it shouldn't be replaced by this routine.
	 */
	if (ERR_SETECODE == errnum)
		return;
	/* When the value of $ECODE is non-empty, error trapping is invoked. When the severity level does not warrant
	 * error trapping, no value should be copied into $ECODE. Note: the message is verified it IS a GTM message before
	 * checking the severity code so system error numbers aren't misinterpreted.
	 */
	severity = errnum & SEV_MSK;
	if ((NULL != err_check(errnum)) && ((INFO == severity) || (SUCCESS == severity)))
		return;
	/* Get ECODE string from error-number. If the error has an ANSI standard code, return ,Mnnn, (nnn is ANSI code).
	 * Always return ,Zxxx, (xxx is GT.M code). Note that the value of $ECODE must start and end with a comma
	 */
	ecode_ptr = &ecode_buff[0];
	*ecode_ptr++ = ',';
	if (ectl = err_check(errnum))
	{
		ansi_error = ((errnum & FACMASK(ectl->facnum)) && (MSGMASK(errnum, ectl->facnum) <= ectl->msg_cnt))
			? error_ansi[MSGMASK(errnum, ectl->facnum) - 1]	: 0;
		if (0 < ansi_error)
		{
			*ecode_ptr++ = 'M';
			ecode_ptr = (char *)i2asc((unsigned char *)ecode_ptr, ansi_error);
			*ecode_ptr++ = ',';
		}
	}
	*ecode_ptr++ = 'Z';
	ecode_ptr = (char *)i2asc((unsigned char *)ecode_ptr, errnum);
	*ecode_ptr++ = ',';
	ecode_mstr.addr = &ecode_buff[0];
	ecode_mstr.len = INTCAST(ecode_ptr - ecode_mstr.addr);
	assert(ecode_mstr.len <= ECODE_MAX_LEN);
	assertpro(ECODE_MAX_LEN_WITH_BUFFER >= ecode_mstr.len);
	ecode_add(&ecode_mstr);
}
