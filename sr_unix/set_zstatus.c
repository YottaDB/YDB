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
#include "gtm_stdio.h"

#include "error.h"
#include "min_max.h"
#include "stringpool.h"
#include "mlkdef.h"
#include "zshow.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mvalconv.h"
#include "error_trap.h"
#include "trans_code_cleanup.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF mval		dollar_zstatus, dollar_zerror;
GBLREF mval		dollar_ztrap, dollar_etrap;
GBLREF stack_frame	*zyerr_frame, *frame_pointer;
GBLREF mstr             *err_act;

error_def(ERR_MEMORY);

unsigned char *set_zstatus(mstr *src, int arg, unsigned char **ctxtp, boolean_t need_rtsloc)
{
	unsigned char	*b_line;	/* beginning of line (used to restart line) */
	mval		val;		/* pointer to dollar_zstatus */
	unsigned char	zstatus_buff[2*OUT_BUFF_SIZE];
	unsigned char	*zstatus_bptr, *zstatus_iter;
	int		save_arg;
	size_t		util_len ;
	mval		*status_loc;
	boolean_t 	trans_frame;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	b_line = 0;
	if (!need_rtsloc)
	 	trans_frame = FALSE;
	else
	{	/* get the line address of the last "known" MUMPS code that was executed.  MUMPS
		 * indirection consitutes MUMPS code that is "unknown" is the sense that there is no
		 * line address for it.
		 */
		trans_frame = !(SFT_DM & frame_pointer->type) && ((!(frame_pointer->type & SFT_COUNT
			|| 0 == frame_pointer->type)) || (SFT_ZINTR & frame_pointer->type));
		if (trans_frame)
		{
			save_arg = arg;
			SET_ERR_CODE(frame_pointer, arg);
		}
		src->len = INTCAST(get_symb_line((unsigned char*)src->addr, &b_line, ctxtp) - (unsigned char*)src->addr);
	}
	MV_FORCE_MVAL(&val, arg);
	n2s(&val);
	memcpy(zstatus_buff, val.str.addr, val.str.len);
	zstatus_bptr = zstatus_buff + val.str.len;
	*zstatus_bptr++ = ',';
	if (NULL != b_line)
	{
		memcpy(zstatus_bptr, src->addr, src->len);
		zstatus_bptr += src->len;
		*zstatus_bptr++ = ',';
	}
	zstatus_iter = zstatus_bptr;
	util_len = TREF(util_outptr) - TREF(util_outbuff_ptr);
	if (trans_frame)
	{	/* currently no inserted message (arg) needs arguments.  The following code needs
		 * to be changed if any new parametered message is added.
		 */
		*(TREF(util_outbuff_ptr)) = '-';
		memcpy(&zstatus_buff[OUT_BUFF_SIZE], TREF(util_outbuff_ptr), util_len); /* save original message */
		util_out_print(NULL, RESET); /* clear any pending msgs and reset util_out_buff */
		gtm_putmsg_noflush(VARLSTCNT(1) arg);

		memcpy(zstatus_bptr, TREF(util_outbuff_ptr), TREF(util_outptr) - TREF(util_outbuff_ptr));
		zstatus_bptr += (TREF(util_outptr) - TREF(util_outbuff_ptr));
		*zstatus_bptr++ = ',';
		memcpy(zstatus_bptr, &zstatus_buff[OUT_BUFF_SIZE], util_len);

		/* restore original message into util_outbuf */
		memcpy(TREF(util_outbuff_ptr), &zstatus_buff[OUT_BUFF_SIZE], util_len);
		*(TREF(util_outbuff_ptr)) = '%';
		TREF(util_outptr) = TREF(util_outbuff_ptr) + util_len;
		arg = save_arg;
	} else
		memcpy(zstatus_bptr, TREF(util_outbuff_ptr), util_len);
	zstatus_bptr += util_len;
	for (; zstatus_iter < zstatus_bptr; zstatus_iter++)
	{
		if ('\n' == *zstatus_iter)
			*zstatus_iter = ',';
	}
	status_loc = (NULL == zyerr_frame) ? &dollar_zstatus : &dollar_zerror;
	status_loc->str.len = INTCAST(zstatus_bptr - zstatus_buff);
	status_loc->str.addr = (char *)zstatus_buff;
	s2pool(&status_loc->str);
	status_loc->mvtype = MV_STR;
        /* If this is a MEMORY issue, setting the ecode is of dubious worth since we are not going
         * to drive any handlers and it can definitely be expensive in terms of memory use as ecode_add()
         * (further down the pike) is likely to load the text of the module into storage if it can. So we bypass
         * ecode setting for these two fatal errors. 02/2008 se
	 */
	if (ERR_MEMORY != arg)
		ecode_set(arg);
	return (b_line);
}
