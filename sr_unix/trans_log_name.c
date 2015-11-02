/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iosp.h"
#include "trans_log_name.h"
#include "send_msg.h"

/* Allocate a buffer to be used for passing a null-terminated environment-variable to GETENV.
 * If more space is needed, we will expand later. Need to statically allocate space to hold "$gtmdbglvl"
 * since this is the first environment variable that will be passed to trans_log_name and needs
 * to be translated BEFORE doing any mallocs hence the initial static allocation of MAX_TRANS_NAME_LEN bytes.
 */
STATICDEF char	trans_log_name_startbuff[MAX_TRANS_NAME_LEN];
STATICDEF int	trans_log_name_buflen = MAX_TRANS_NAME_LEN - 1;	/* length of buffer currently allocated */
STATICDEF char	*trans_log_name_buff = &trans_log_name_startbuff[0];

int4 trans_log_name(mstr *log, mstr *trans, char *buffer, int4 buffer_len, translog_act do_sendmsg)
{
	char	*s_start, *s_ptr, *s_top, *tran_buff, *b_ptr, *b_top, ch;
	int	len, s_len;
	int4	ret;

	error_def(ERR_LOGTOOLONG);

	ret = SS_NOLOGNAM; /* assume we don't find it */
	b_ptr = buffer; /* b_ptr is points to next place to fill in in output buffer */
	b_top = buffer + buffer_len;
	s_start = log->addr;
	s_top = s_start + log->len;
	for (s_ptr = s_start; s_ptr < s_top; )
	{
		assert(s_ptr != buffer); /* should be no intersections between input and output buffer at any point of processing */
		if ('$' == *s_ptr)
		{	/* We hit a env var that needs to be translated - they start with $ */

			/* For non-initial pass copy any non-env var text that we have passed over into output buffer
			 * and update output buffer pointer. Before that check if output buffer can hold that data.
			 */
			s_len = (int)(s_ptr - s_start);
			if ((b_ptr + s_len) >= b_top)
			{
				ret = SS_LOG2LONG;
				break;
			}
			memcpy(b_ptr, s_start, s_len);
			b_ptr += s_len;
			assert(b_ptr < b_top);
			/* Move forward in input buffer (over text just processed) */
			s_start = s_ptr++;
			/* Get the env var name. Take care not to exceed input string length */
			for ( ; (s_ptr < s_top) && (ch = *s_ptr, ('_' == ch) || ISALNUM_ASCII(ch)); s_ptr++)
				;
			s_len = (int)(s_ptr - s_start) - 1;
			/* Copy it into "temporary-buffer" so we can null-terminate it and pass to GETENV */
			if (trans_log_name_buflen <= s_len)
			{	/* Currently allocated buffer is not enough. Expand it. */
				assert(NULL != trans_log_name_buff);
				if (trans_log_name_buff != trans_log_name_startbuff)	/* do not free static starting buffer */
					free(trans_log_name_buff);
				trans_log_name_buff = malloc(s_len + 1);
				trans_log_name_buflen = s_len + 1;
			}
			memcpy(trans_log_name_buff, s_start + 1, s_len);
			trans_log_name_buff[s_len] = 0;
			/* try to convert it */
			if (NULL != (tran_buff = GETENV(trans_log_name_buff)))
			{
				s_start = tran_buff;
				s_len = STRLEN(tran_buff);
				ret = SS_NORMAL;
			} else
			{	/* if there is no env var then just copy the name of the env var name including $ */
				s_len = (int)(s_ptr - s_start);
			}
			if ((b_ptr + s_len) >= b_top)
			{
				ret = SS_LOG2LONG;
				break;
			}
			memcpy(b_ptr, s_start, s_len);
			b_ptr += s_len;
			assert(b_ptr < b_top);
			/* move over env var just processed */
			s_start = s_ptr;
		} else
			s_ptr++;	/* keep going until you either hit end of string or $ */
	}
	if (SS_LOG2LONG != ret)
	{	/* if there is anything left after the last env var name, copy it */
		s_len = (int)(s_ptr - s_start);
		if ((b_ptr + s_len) >= b_top)
			ret = SS_LOG2LONG;
		else
		{
			memcpy(b_ptr, s_start, s_len);
			b_ptr += s_len;
			assert(b_ptr < b_top);
		}
	}
	assert(b_ptr < b_top);	/* "<" instead of "<=" so it is safe to write the termination '\0' character */
	/* create the return mstr */
	trans->addr = buffer;
	trans->len = INTCAST(b_ptr - buffer);
	assert(trans->len < buffer_len);
	/* Null-terminate returned string (even though an mstr), as this is relied upon
	 * by callers who do ATOI etc. directly on the return string.
	 */
	trans->addr[trans->len] = '\0';
	if (do_sendmsg && (SS_LOG2LONG == ret))
		send_msg(VARLSTCNT(5) ERR_LOGTOOLONG, 3, log->len, log->addr, buffer_len - 1);	/* - 1 for terminating null byte */
	return ret;
}
