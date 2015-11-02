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

#include "mlkdef.h"
#include "zshow.h"
#include "patcode.h"
#include "compiler.h"	/* for CHARMAXARGS */
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"	/* for POP_MV_STENT */

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#include "gtm_icu_api.h"	/* U_ISPRINT() needs this header */
#endif

GBLREF	uint4		*pattern_typemask;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF	int		process_exiting;

LITDEF MSTR_CONST(quote, QUOTE);
LITDEF MSTR_CONST(quote_concat, QUOTE_CONCAT);
LITDEF MSTR_CONST(close_paren, CLOSE_PAREN);
LITDEF MSTR_CONST(close_paren_quote, CLOSE_PAREN_QUOTE);

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

/* The following macro prints the last contiguous sequence of printable (graphic) characters
 * that have been processed so far to the zshow buffer. It also resets the size of the next
 * next sequence that may follow */
#define ZWR_PRINTABLE						\
{								\
	graphic_str->str.len = src_cnt - strstart;		\
	graphic_str->str.addr = v->str.addr + strstart;		\
	zshow_output(output, &graphic_str->str);		\
}

/* The routine that does formatting for ZWRITE command.
 * NOTE: this routine does almost the same formatting as format2zwr(). However,
 * the main difference is this routine feeds the formatted results to the zshow
 * output handling routine (zshow_output()) instead of storing in a pre-allocated
 * destination buffer */
void mval_write(zshow_out *output, mval *v, boolean_t flush)
{
        sm_uc_ptr_t	cp;
	uint4		ch;
	int		fastate = 0, ncommas, src_len, src_cnt, strstart, chlen;
	boolean_t	isctl, isill;
	char		*strnext;
	mval		*graphic_str, lmv;

	MV_FORCE_STR(v);
	src_len = v->str.len;
	if (src_len > 0)
	{
		if (val_iscan(v))
		{
			output->flush = flush;
			zshow_output(output, &v->str);
			return;
		}
		fastate = 0;
		strstart = 0;

		if (!process_exiting)
		{ /* Only if not exiting in case we are called from mdb_condition_handler with stack overflow */
			PUSH_MV_STENT(MVST_MVAL);
			graphic_str = &mv_chain->mv_st_cont.mvs_mval;
		} else
			graphic_str = &lmv;
		graphic_str->mvtype = MV_STR;
		graphic_str->str.len = 0; /* initialize len in case stp_gcol gets called before actual set of len occurs below */

		/* Note that throughout this module, we use v->str.addr[xxx] to access the input string instead of
		 * maintaining a pointer variable. This is because we call zshow_output at lot of places and each
		 * of them can invoke stp_gcol in turn changing the value of v->str.addr.
		 */
		for (src_cnt = 0; src_cnt < src_len; src_cnt += chlen)
		{
			if (!gtm_utf8_mode)
			{
				ch = (unsigned char)v->str.addr[src_cnt];
				isctl = ((pattern_typemask[ch] & PATM_C) != 0);
				isill = FALSE;
				chlen = 1;
			}
			UNICODE_ONLY(
			else
			{
				strnext = (char *)UTF8_MBTOWC(&v->str.addr[src_cnt], &v->str.addr[src_len], ch);
				isill = (WEOF == ch) ? (ch = (unsigned char)v->str.addr[src_cnt], TRUE) : FALSE;
				if (!isill)
					isctl = !U_ISPRINT(ch);
				chlen = (int)(strnext - &v->str.addr[src_cnt]);
			}
			)
			switch(fastate)
			{
				case 0:	/* beginning of the string */
				case 1: /* beginning of a new substring followed by a graphic character */
					if (isill)
					{
						if (src_cnt > 0)
						{ 	/* close previous string with quote and prepare for
							   concatenation */
							ZWR_PRINTABLE;
							zshow_output(output, &quote_concat);
						}
						mval_nongraphic(output, LIT_AND_LEN(DOLLARZCH), ch);
						fastate = 3;
						ncommas = 0;
					} else if (isctl)
					{
						if (src_cnt > 0)
						{	/* close previous string with quote and prepare for
							   concatenation */
							ZWR_PRINTABLE;
							zshow_output(output, &quote_concat);
						}
						mval_nongraphic(output, LIT_AND_LEN(DOLLARCH), ch);
						fastate = 2;
						ncommas = 0;
					} else
					{	/* graphic characters */
						if (0 == fastate) /* the initial quote in the beginning */
						{
							zshow_output(output, &quote);
							fastate = 1;
						}
						if ('"' == ch)
						{
							ZWR_PRINTABLE;
							zshow_output(output, &quote);
							strstart = src_cnt;
						}
					}
					break;
				case 2: /* subsequent characters following a non-graphic character in the form
					   of $CHAR(x,) */
					if (isill)
					{
						mval_nongraphic(output, LIT_AND_LEN(CLOSE_PAREN_DOLLARZCH), ch);
						fastate = 3;
					} else if(isctl)
					{
						ncommas++;
						if (CHARMAXARGS == ncommas)
						{
							ncommas = 0;
							mval_nongraphic(output, LIT_AND_LEN(CLOSE_PAREN_DOLLARCH), ch);
						} else
							mval_nongraphic(output, LIT_AND_LEN(COMMA), ch);
					} else
					{
						zshow_output(output, &close_paren_quote);
						if ('"' == ch)
							zshow_output(output, &quote);
						strstart = src_cnt;
						fastate = 1;
					}
					break;
				case 3: /* subsequent characters following an illegal character in the form of $ZCHAR(x,) */
					if(isill)
					{
						ncommas++;
						if (CHARMAXARGS == ncommas)
						{
							ncommas = 0;
							mval_nongraphic(output, LIT_AND_LEN(CLOSE_PAREN_DOLLARZCH), ch);
						} else
							mval_nongraphic(output, LIT_AND_LEN(COMMA), ch);
					} else if (isctl)
					{
						mval_nongraphic(output, LIT_AND_LEN(CLOSE_PAREN_DOLLARCH), ch);
						fastate = 2;
					} else
					{
						zshow_output(output, &close_paren_quote);
						if ('"' == ch)
							zshow_output(output, &quote);
						strstart = src_cnt;
						fastate = 1;
					}
					break;
				default:
					assert(FALSE);
					break;
			}
		}

		/* close up */
		switch(fastate)
		{
			case 1:
				ZWR_PRINTABLE;
				zshow_output(output, &quote);
				break;
			case 2:
			case 3:
				zshow_output(output, &close_paren);
				break;
			default:
				assert(FALSE);
				break;
		}
		if (!process_exiting)
		{
			POP_MV_STENT();
		}
	} else
	{
		zshow_output(output, &quote);
		zshow_output(output, &quote);
	}
	if (flush)
	{
		output->flush = TRUE;
		zshow_output(output, 0);
	}
}
