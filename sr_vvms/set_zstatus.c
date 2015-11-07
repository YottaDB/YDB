/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <chfdef.h>
#include <descrip.h>

#include "stringpool.h"
#include "mlkdef.h"
#include "zshow.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mvalconv.h"
#include "error_trap.h"
#include "trans_code_cleanup.h"

GBLREF mval		dollar_zstatus, dollar_zerror;
GBLREF mval             dollar_ztrap, dollar_etrap;
GBLREF spdesc           rts_stringpool, stringpool;
GBLREF stack_frame	*zyerr_frame, *frame_pointer;
GBLREF mstr             *err_act;

#define	DSZ_BUF_SIZ	512

static short zs_size;
int put_zstatus(struct dsc$descriptor_s *txt, unsigned char *buffer);

unsigned char *set_zstatus(mstr *src, struct chf$signal_array *sig, unsigned char **ctxtp, boolean_t need_rtsloc)
{
	unsigned char	*b_line;	/* beginning of line (used to restart line) */
	mval		val;		/* dummy mval */
	int4		save_sig_args, save_sig_name;
	unsigned char	zstatus_buff[DSZ_BUF_SIZ];
	mval		*status_loc;
	boolean_t	trans_frame;
	short 		save_zs_size;
	error_def(ERR_VMSMEMORY);

	b_line = 0;
	if (need_rtsloc)
	{
		/* get the line address of the last "known" MUMPS code that was executed.  MUMPS indirection
		 * consitutes MUMPS code that is "unknown" is the sense that there is no line address for it.
		 */
		src->len = get_symb_line((unsigned char*)src->addr, &b_line, ctxtp) - (unsigned char*)src->addr;
		trans_frame = (!(SFT_DM & frame_pointer->type) &&
			       ((!(frame_pointer->type & SFT_COUNT || 0 == frame_pointer->type)) ||
			        (SFT_ZINTR & frame_pointer->type)));
		if (trans_frame)
		{
			save_sig_name = sig->chf$l_sig_name;
			SET_ERR_CODE(frame_pointer, sig->chf$l_sig_name);
		}
	} else
		trans_frame = FALSE;
	MV_FORCE_MVAL(&val, sig->chf$l_sig_name) ;
	n2s(&val);
	memcpy(zstatus_buff, val.str.addr, val.str.len);
	zs_size = val.str.len;
	zstatus_buff[zs_size++] = ',';
	if (0 != b_line)
	{
		memcpy(&(zstatus_buff[zs_size]), src->addr, src->len);
		zs_size += src->len;
	}
	save_sig_args = sig->chf$l_sig_args;
	assert(2 < sig->chf$l_sig_args);
	sig->chf$l_sig_args -= 2;
	sig->chf$l_sig_args |= 0x000F0000;
	if (trans_frame)
	{ /* currently no inserted message (sig->chf$l_sig_name) needs arguments.
	     The following code needs to be changed for any new message with arguments */
		sys$putmsg(sig, put_zstatus, 0, zstatus_buff);
		save_zs_size = zs_size;
		sig->chf$l_sig_name = save_sig_name;
		sys$putmsg(sig, put_zstatus, 0, zstatus_buff);
		zstatus_buff[save_zs_size + 1] = '-'; /* auxiliary msgs need prefix '-' instead of '%' */
	} else
		sys$putmsg(sig, put_zstatus, 0, zstatus_buff);
	sig->chf$l_sig_args = save_sig_args;
	status_loc = (NULL == zyerr_frame) ? &dollar_zstatus : &dollar_zerror;
	status_loc->str.len = zs_size;
	status_loc->str.addr = zstatus_buff;
	assert(stringpool.base == rts_stringpool.base);
	s2pool(&status_loc->str);
	status_loc->mvtype = MV_STR;
	/* If this is a VMSMEMORY issue, setting the ecode is of dubious worth since we are not going
	   to drive any handlers and it can definitely be expensive in terms of memory use as ecode_add()
	   (further down the pike) is likely to load the text of the module into storage if it can. So we bypass
	   ecode setting for these two fatal errors. 02/2008 se
	*/
	if (ERR_VMSMEMORY != sig->chf$l_sig_name)
		ecode_set(sig->chf$l_sig_name);
	return (b_line);
}

int put_zstatus(struct dsc$descriptor_s *txt, unsigned char *buffer)
{
	short msg_len;

	assert(DSZ_BUF_SIZ >= zs_size);
	msg_len = (zs_size + 1 + txt->dsc$w_length < DSZ_BUF_SIZ ? 1 + txt->dsc$w_length : DSZ_BUF_SIZ - zs_size);
	if (msg_len)
	{
		*(buffer + zs_size++) = ',';
		memcpy(buffer + zs_size, txt->dsc$a_pointer, txt->dsc$w_length);
		zs_size += txt->dsc$w_length;
		assert(DSZ_BUF_SIZ >= zs_size);
	}
	return 0;	/* suppress display on SYS$OUTPUT */
}
