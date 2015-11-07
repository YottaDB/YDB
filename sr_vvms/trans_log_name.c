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
#include <lnmdef.h>
#include <ssdef.h>

#include "io.h"
#include "iottdef.h"
#include "trans_log_name.h"

static $DESCRIPTOR(tables,"LNM$FILE_DEV");
static $DESCRIPTOR(lognam,"");
static $DESCRIPTOR(colon,":");
static $DESCRIPTOR(src_str,"");
static uint4 attr = LNM$M_CASE_BLIND;

#define TRL_SZ	3
#define MAX_TRAN_DEPTH 10

int4 trans_log_name(mstr *log, mstr *trans, char *buffer)
/* 1st arg: logical name    */
/* 2nd arg: translated name */
{
	item_list_struct trl_list[TRL_SZ];
	char		buff1[MAX_TRANS_NAME_LEN], buff2[MAX_TRANS_NAME_LEN], tail_buff[MAX_TRANS_NAME_LEN];
	char		*temp_buffer, *tail_ptr;
	int4		status, max_index, x, new_pos;
	uint4		attr_mask, pos, tail_len;
	unsigned short	ret_len, pass;

	error_def(ERR_INVSTRLEN);

	if (MAX_TRANS_NAME_LEN < log->len)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, log->len, MAX_TRANS_NAME_LEN);
	trl_list[0].buf_len = SIZEOF(attr_mask);
	trl_list[0].item_code = LNM$_ATTRIBUTES;
	trl_list[0].addr = &attr_mask;
	trl_list[0].ret_addr = &ret_len;
	trl_list[1].buf_len = MAX_TRANS_NAME_LEN;
	trl_list[1].item_code = LNM$_STRING;
	trl_list[1].addr = buff1;
	trl_list[1].ret_addr = &ret_len;
	trl_list[2].buf_len = 0;
	trl_list[2].item_code = 0;

	src_str.dsc$a_pointer = log->addr;
	src_str.dsc$w_length = log->len;

	tail_ptr = &tail_buff[MAX_TRANS_NAME_LEN];
	tail_len = 0;

	for (pass = 0; ;pass++)
	{
		ret_len = 0;
		attr_mask = 0;
		pos = lib$locc(&colon,&src_str);
		if (pos != 0 && pos != log->len)
		{
			tail_len += src_str.dsc$w_length - pos;
			if (tail_len > MAX_TRANS_NAME_LEN)
				rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, tail_len, MAX_TRANS_NAME_LEN);
			tail_ptr -= src_str.dsc$w_length - pos;
			memcpy(tail_ptr, src_str.dsc$a_pointer + pos, src_str.dsc$w_length - pos);
			src_str.dsc$w_length = pos - 1;
		} else
			pos = 0;
		status = sys$trnlnm(&attr ,&tables ,&src_str,0,trl_list);
		if ((status & 1) || (status == SS$_NOLOGNAM && pass))
		{
			if (attr_mask & LNM$M_TERMINAL)
			{
				memcpy(buffer, trl_list[1].addr, ret_len);
				trans->addr = buffer;
				trans->len = ret_len;
				if (tail_len)
				{
					if (tail_len + trans->len  > MAX_TRANS_NAME_LEN)
						rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, tail_len, MAX_TRANS_NAME_LEN);
					memcpy(trans->addr + trans->len, tail_ptr, tail_len);
					trans->len += tail_len;
				}
				/* Null-terminate returned string (even though an mstr), as this is relied upon
				 * by callers who do ATOI etc. directly on the return string.
				 */
				trans->addr[trans->len] = '\0';
				return (SS$_NORMAL);
			}
			if (status == SS$_NOLOGNAM || pass > MAX_TRAN_DEPTH)
			{
				memcpy(buffer, src_str.dsc$a_pointer, src_str.dsc$w_length);
				trans->addr = buffer;
				trans->len = src_str.dsc$w_length;
				if (pos)
				{
					if (trans->len + 1 > MAX_TRANS_NAME_LEN)
						rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, trans->len + 1, MAX_TRANS_NAME_LEN);
					*((unsigned char *) trans->addr + trans->len) = ':';
					trans->len++;
				}
				if (tail_len)
				{
					if (tail_len + trans->len  > MAX_TRANS_NAME_LEN)
						rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, tail_len + trans->len, MAX_TRANS_NAME_LEN);
					memcpy(trans->addr + trans->len, tail_ptr, tail_len);
					trans->len += tail_len;
				}
				/* Null-terminate returned string (even though an mstr), as this is relied upon
				 * by callers who do ATOI etc. directly on the return string.
				 */
				trans->addr[trans->len] = '\0';
				return (SS$_NORMAL);
			}
			temp_buffer = pass ? src_str.dsc$a_pointer : buff2;
			src_str.dsc$a_pointer = trl_list[1].addr;
			src_str.dsc$w_length = ret_len;
			trl_list[1].addr = temp_buffer;
		} else
		{	trans->addr = 0;
			trans->len = 0;
			return(status);
		}
	}
}
