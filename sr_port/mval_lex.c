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
#include "stringpool.h"

GBLREF spdesc stringpool;

static readonly char dollarch[] = "$C(";
static readonly char quote_dch[] = "\"_$C(";
static readonly char close_paren_quote[] = ")_\"";

void mval_lex(mval *v,mstr *output)
{
	unsigned char *cp,*top,*stp;
	int fastate, space_needed;
	bool isctl;
	int ch, n;
	mstr one;
	char *numptr, buff[3];	/* numeric conversion buffer */
	mstr tmpmstr;

	MV_FORCE_STR(v);
	if (MV_IS_CANONICAL(v))
	{
		*output = v->str;
	}else
	{
		space_needed = 0;
		fastate = -1;
		for (cp = (unsigned char *)v->str.addr , top = cp + v->str.len ; top > cp ; )
		{
			space_needed++;
			ch = *cp++;
			if (ch < 0x20 || ch == '\"' || ch > 0x7e)
			{
				/* special character formatting required
				fa for processing controlled by state variable fastate
					int ch = -1 means end of string
					isctl means is a control character
				*/
				assert(v->str.len);
				space_needed = 0;
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
						isctl = (ch < 0x20 || ch > 0x7e);
					}
					switch (fastate)
					{
					case 0:	/* first time through */
						if (isctl)
						{
							space_needed += sizeof(dollarch) - 1;
							n = ch / 100;
							if (n)
								space_needed++;
							n = ch / 10;
							if (n)
								space_needed++;
							space_needed++;
							fastate = 2;
						} else
						{
							space_needed++;
							tmpmstr.addr = (char *)cp - 1;
							fastate = 1;
							if (ch == '\"')
								space_needed++;
						}
						break;
					case 1:	/* graphic characters */
						if (ch == -1 || isctl)
						{
							tmpmstr.len = (char *) cp - tmpmstr.addr - 1;
							space_needed += tmpmstr.len;
							if (!isctl)
								space_needed++;
							else
							{
								space_needed += sizeof(quote_dch)-1;
								n = ch / 100;
								if (n)
									space_needed++;
								n = ch / 10;
								if (n)
									space_needed++;
								space_needed++;
								fastate = 2;
							}
						} else if (ch == '\"')
						{
							tmpmstr.len = (char *)cp - tmpmstr.addr;
							space_needed += tmpmstr.len;
							tmpmstr.addr = (char *)cp - 1;
						}
						break;
					case 2:	/* subsequent non-graphics*/
						if (ch == -1)
						{	space_needed++;
						} else if (isctl || ch == '\"')
						{
							space_needed++;		/* comma */
							n = ch / 100;
							if (n)
								space_needed++;
							n = ch / 10;
							if (n)
								space_needed++;
							space_needed++;
						} else
						{
							space_needed += sizeof(close_paren_quote) - 1;
							tmpmstr.addr = (char *)cp - 1;
							fastate = 1;
						}
						break;
					default:
						assert(FALSE);
						break;
					}
				} while (ch != -1);
			}
		}
		if (fastate == -1)			/* no translation needed */
			space_needed += 2;		/* quotation marks */
		if (stringpool.free + space_needed > stringpool.top)
			stp_gcol(space_needed);
		stp = stringpool.free;

		if (fastate == -1)			/* no translation needed */
		{
			assert(space_needed == v->str.len + 2);
			*stp++ = '"';
			memcpy(stp, v->str.addr, v->str.len);
			stp += v->str.len;
			*stp++ = '"';
			output->addr = (char *)stringpool.free;
			output->len = v->str.len + 2;
		}
		else
		{	/* special character formatting required
			fa for processing controlled by state variable fastate
				int ch = -1 means end of string
				isctl means is a control character
			*/
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
					isctl = (ch < 0x20 || ch > 0x7e);
				}
				switch (fastate)
				{
				case 0:	/* first time through */
					if (isctl)
					{
						memcpy(stp, dollarch, sizeof(dollarch)-1);
						stp += sizeof(dollarch)-1;
						n = ch / 100;
						if (n)
						{	*stp++ = n + '0';
							ch -= n * 100;
						}
						n = ch / 10;
						if (n)
						{	*stp++ = n + '0';
							ch -= n * 10;
						}
						*stp++ = ch + '0';
						fastate = 2;
					} else
					{
						*stp++ = '"';
						tmpmstr.addr = (char *)cp - 1;
						fastate = 1;
						if (ch == '\"')
							*stp++ = '"';
					}
					break;
				case 1:	/* graphic characters */
					if (ch == -1 || isctl)
					{
						tmpmstr.len = (char *) cp - tmpmstr.addr - 1;
						memcpy(stp, tmpmstr.addr, tmpmstr.len);
						stp += tmpmstr.len;
						if (!isctl)
							*stp++ = '"';
						else
						{
							memcpy(stp, quote_dch, sizeof(quote_dch)-1);
							stp += sizeof(quote_dch)-1;
							n = ch / 100;
							if (n)
							{	*stp++ = n + '0';
								ch -= n * 100;
							}
							n = ch / 10;
							if (n)
							{	*stp++ = n + '0';
								ch -= n * 10;
							}
							*stp++ = ch + '0';
							fastate = 2;
						}
					} else if (ch == '\"')
					{
						tmpmstr.len = (char *)cp - tmpmstr.addr;
						memcpy(stp, tmpmstr.addr, tmpmstr.len);
						stp += tmpmstr.len;
						tmpmstr.addr = (char *)cp - 1;
					}
					break;
				case 2:	/* subsequent non-graphics*/
					if (ch == -1)
					{
						*stp++ = ')';
					} else if (isctl || ch == '\"')
					{
						*stp++ = ',';
						n = ch / 100;
						if (n)
						{	*stp++ = n + '0';
							ch -= n * 100;
						}
						n = ch / 10;
						if (n)
						{	*stp++ = n + '0';
							ch -= n * 10;
						}
						*stp++ = ch + '0';
					} else
					{
						memcpy(stp, close_paren_quote, sizeof(close_paren_quote) - 1);
						stp += sizeof(close_paren_quote) - 1;
						tmpmstr.addr = (char *)cp - 1;
						fastate = 1;
					}
					break;
				default:
					assert(FALSE);
					break;
				}
			} while (ch != -1);
			output->addr = (char *)stringpool.free;
			output->len = stp - stringpool.free;
			assert(space_needed == output->len);
		}
	}
}
