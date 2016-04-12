/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gtm_string.h"

#include "gtm_multi_thread.h"
#include "util.h"		/* for FLUSH and util_out_print */
#include "util_out_print_vaparm.h"
#include "op.h"			/* for op_write prototype */
#include "io.h"			/* needed for io_pair typedef */
#include "gtmimagename.h"	/* for IS_MCODE_RUNNING */

GBLREF	uint4			dollar_tlevel;

#define	ZTRIGBUFF_INIT_ALLOC		1024	/* start at 1K */
#define	ZTRIGBUFF_INIT_MAX_GEOM_ALLOC	1048576	/* stop geometric growth at this value */

/* Used by MUPIP TRIGGER or $ZTRIGGER routines to buffer trigger output until TCOMMIT time
 * (as otherwise we might display stale output due to a restarted try).
 */
void	util_out_print_gtmio(caddr_t message, int flush, ...)
{
	va_list		var;
	char		*src, *dst, *newdst;
	int		srclen, dstlen, dstalloc, newlen, ptrlen;
	caddr_t		msg;
	int4		msglen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	/* we expect all trigger operations (SELECT, LOAD, etc.) to happen inside TP. exceptions should set TREF variable */
	assert(dollar_tlevel || TREF(gtmio_skip_tlevel_assert));
	va_start(var, flush);
	/* If "!AD" has been specified as the message, skip the util_out_print_vaparm call as it is possible
	 * the input string length is > the maximum length util_out_* routines are designed to handle.
	 * In that case, we dont need the parameter substitution anyways so do a memcpy instead.
	 */
	if (STRCMP(message, "!AD") || (FLUSH != flush))
	{
		util_out_print_vaparm(message, NOFLUSH, var, MAXPOSINT4);
		src = TREF(util_outbuff_ptr);
		assert(NULL != TREF(util_outptr));
		srclen = INTCAST(TREF(util_outptr) - src) + 1;	/* 1 is for '\n' */
		assert(OUT_BUFF_SIZE >= srclen);
	} else
	{
		srclen = (int)va_arg(var, int4) + 1;
		src = (char *)va_arg(var, caddr_t);
	}
	if (FLUSH == flush)
	{
		dstalloc = TREF(ztrigbuffAllocLen);
		dstlen = TREF(ztrigbuffLen);
		/* Leave room for terminating '\0' (after the \n) for later use in FPRINTF in tp_ztrigbuff_print.
		 * Hence the use of "<=" instead of a "<" in the "if (dstalloc <= (dstlen + srclen))" check below.
		 */
		if (dstalloc <= (dstlen + srclen))
		{	/* reallocate */
			dst = TREF(ztrigbuff);
			do
			{
				if (!dstalloc)
					dstalloc = ZTRIGBUFF_INIT_ALLOC;	/* Allocate a 1K buffer at start */
				else if (ZTRIGBUFF_INIT_MAX_GEOM_ALLOC <= dstalloc)
					dstalloc += ZTRIGBUFF_INIT_MAX_GEOM_ALLOC;
				else
					dstalloc = dstalloc * 2; /* grow geometrically until a limit and linearly after that */
			} while (dstalloc <= (dstlen + srclen));
			newdst = malloc(dstalloc);
			if (dstlen)
				memcpy(newdst, dst, dstlen);
			TREF(ztrigbuff) = newdst;
			TREF(ztrigbuffAllocLen) = dstalloc;
			if (NULL != dst)
				free(dst);
		}
		dst = TREF(ztrigbuff);
		memcpy(dst + dstlen, src, srclen - 1);
		dst[dstlen + srclen - 1] = '\n';
		TREF(ztrigbuffLen) += srclen;
		TREF(util_outptr) = TREF(util_outbuff_ptr);	/* Signal text is flushed */
	}
	va_end(TREF(last_va_list_ptr));
	va_end(var);
}

void	tp_ztrigbuff_print(void)
{
	mval		flushtxt;
	char		*ptr, *ptrtop, *ptr2;
	int		len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If mumps process doing trigger operations, use GTM IO routines. If not use util_out_print. The only
	 * exception is if the MUPIP caller wants to use current IO device (i.e. it has it all set up). In that
	 * case the TREF ztrig_use_io_curr_device would have been set to TRUE.
	 */
	if (IS_MCODE_RUNNING || TREF(ztrig_use_io_curr_device))
	{
		ptr = TREF(ztrigbuff);
		assert('\n' == ptr[TREF(ztrigbuffLen) - 1]);
		ptrtop = ptr + TREF(ztrigbuffLen);
		flushtxt.mvtype = MV_STR;
		do
		{
			len = INTCAST(ptrtop - ptr);
			ptr2 = memchr(ptr, '\n', len);
			assert(NULL != ptr2);
			flushtxt.str.addr = ptr;
			flushtxt.str.len = ptr2 - ptr;
			op_write(&flushtxt);
			op_wteol(1);
			ptr = ptr2 + 1;
		} while (ptr < ptrtop);
	} else
	{	/* Use util_out_print but since you pass TRUE for flush, use -1 to prevent duplicate newline */
		assert('\n' == (TREF(ztrigbuff))[TREF(ztrigbuffLen) - 1]);
		assert(TREF(ztrigbuffLen) < TREF(ztrigbuffAllocLen));
		(TREF(ztrigbuff))[TREF(ztrigbuffLen)] = '\0';
		FPRINTF(stderr, "%s", TREF(ztrigbuff));
	}
}
#endif
