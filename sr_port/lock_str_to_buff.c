/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include "mlkdef.h"
#include "lock_str_to_buff.h"
#include "gtm_string.h"
#include "zshow.h"

/* Takes a lock as input and outputs lock name to string to buff (terminated with NULL). This is called from op_lock2.
 * This function consolidates "output" variable initialization and name string formatting into one function.
 */
void lock_str_to_buff(mlk_pvtblk *pvt_ptr, char *outbuff, int outbuff_len)
{
	mval v;
	zshow_out output;

	memset(&output, 0, SIZEOF(output));
	output.type = ZSHOW_BUFF_ONLY; /* This setting only changes out->ptr the other fileds are ignored */
	output.buff =  &outbuff[0];
	output.size = outbuff_len;
	output.ptr = output.buff;
	zshow_format_lock(&output,pvt_ptr);
	*output.ptr = '\0';
	return;
}
