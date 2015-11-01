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
#include "mlkdef.h"
#include "zshow.h"
#include "patcode.h"
#include "compiler.h"	/* for CHARMAXARGS */
#include "val_iscan.h"	/* for CHARMAXARGS */

GBLREF uint4 *pattern_typemask;

static readonly char dollarch[] = "$C(";
static readonly char quote_dch[] = "\"_$C(";
static readonly char comma[] = ",";
static readonly char close_paren_quote[] = ")_\"";
static readonly char close_paren_dollarch[] = ")_$C(";

void mval_write(zshow_out *output, mval *v, bool flush)
{
	unsigned char	*cp, *top;
	int		fastate, ncommas;
	bool		isctl;
	int		ch;
	mstr		one;
	char		buff;
	mval		tmpmval;

	one.addr = &buff;
	one.len = 1;
	MV_FORCE_STR(v);
	if (val_iscan(v))
	{	output->flush = flush;
		zshow_output(output,&v->str);
	}else
	{
		for (cp = (unsigned char *)v->str.addr , top = cp + v->str.len ; top > cp ; )
		{
			ch = *cp++;
			if (ch == '\"' || (pattern_typemask[ch] & PATM_C))
			{
				/* special character formatting required
				fa for processing controlled by state variable fastate
					int ch = -1 means end of string
					isctl means is a control character
				*/
				tmpmval.mvtype = MV_STR;
				assert(v->str.len);
				fastate = 0;
				cp = (unsigned char *)v->str.addr;
				do
				{
					if (cp >= top)
					{	ch = -1;
						cp++;
					}
					else
					{
						ch = *cp++;
						isctl = ((pattern_typemask[ch] & PATM_C) != 0);
					}
					switch (fastate)
					{
					case 0:	/* first time through */
						if (isctl)
						{
							mval_nongraphic(output, LIT_AND_LEN(dollarch), ch);
							fastate = 2;
							ncommas = 0;
						} else
						{
							*one.addr = '"';
							zshow_output(output,&one);
							tmpmval.str.addr = (char *)cp - 1;
							fastate = 1;
							if (ch == '\"')
							{	*one.addr = '"';
								zshow_output(output,&one);
							}
						}
						break;
					case 1:	/* graphic characters */
						if (ch == -1 || isctl)
						{
							tmpmval.str.len = (char *) cp - tmpmval.str.addr - 1;
							zshow_output(output,&tmpmval.str);
							if (!isctl)
							{	*one.addr = '"';
								zshow_output(output,&one);
							}
							else
							{
								mval_nongraphic(output, LIT_AND_LEN(quote_dch), ch);
								fastate = 2;
								ncommas = 0;
							}
						} else if (ch == '\"')
						{
							tmpmval.str.len = (char *)cp - tmpmval.str.addr;
							zshow_output(output,&tmpmval.str);
							tmpmval.str.addr = (char *)cp - 1;
						}
						break;
					case 2:	/* subsequent non-graphics*/
						if (ch == -1)
						{
							*one.addr = ')';
							zshow_output(output,&one);
						} else if (isctl || ch == '\"')
						{
							ncommas++;
							if (CHARMAXARGS == ncommas)
							{
								ncommas = 0;
								mval_nongraphic(output,
									LIT_AND_LEN(close_paren_dollarch), ch);
							}
						    	else
								mval_nongraphic(output, LIT_AND_LEN(comma), ch);
						} else
						{
							tmpmval.str.addr = close_paren_quote;
							tmpmval.str.len = sizeof(close_paren_quote) - 1;
							zshow_output(output,&tmpmval.str);
							tmpmval.str.addr = (char *)cp - 1;
							fastate = 1;
						}
						break;
					default:
						assert(FALSE);
						break;
					}
				} while (ch != -1);
				if (flush)
				{	output->flush = TRUE;
					zshow_output(output,0);
				}
				return;
			}
		}
		*one.addr = '"';
		zshow_output(output,&one);
		zshow_output(output,&v->str);
		*one.addr = '"';
		output->flush = flush;
		zshow_output(output,&one);
		return;
	}
}
