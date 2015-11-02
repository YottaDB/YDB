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

#include "xfer_enum.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"

#define ZDATE_MAX_LEN	64
#define MAX_YEAR_DIGITS	6
#define DEFAULT1	"MM/DD/YY"
#define DEFAULT2	"DD-MON-YY"
#define DEFAULT3	"MM/DD/YEAR"

GBLREF	spdesc		stringpool;
GBLREF	boolean_t	gtm_utf8_mode;

error_def(ERR_ZDATEFMT);

void op_fnzdate(mval *src, mval *fmt, mval *mo_str, mval *day_str, mval *dst)
{
	int 		date;
	int 		n, nlen;
	int 		outlen;
	int 		year, month, day, time, dow, cent;
	mval 		temp_mval;
	unsigned char 	*i;
	unsigned char 	ch;
	unsigned char 	*fmtptr, *fmttop;
	unsigned char 	*outptr, *outtop, *outpt1;

	static readonly unsigned char montab[] = {31,28,31,30,31,30,31,31,30,31,30,31};
	static readonly unsigned char default1[] = DEFAULT1;
	static readonly unsigned char default2[] = DEFAULT2;
	static readonly unsigned char default3[] = DEFAULT3;
	static readonly unsigned char defmonlst[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	static readonly unsigned char defdaylst[] = "SUNMONTUEWEDTHUFRISAT";
#if defined(BIGENDIAN)
	static readonly int  comma = (((int)',') << 24);
#else
	static readonly int  comma = ',';
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_NUM(src);
	MV_FORCE_STR(fmt);
	MV_FORCE_STR(mo_str);
	MV_FORCE_STR(day_str);
	ENSURE_STP_FREE_SPACE(ZDATE_MAX_LEN);
	time = 0;
	if ((src->mvtype & MV_STR) && (src->mvtype & MV_NUM_APPROX))
	{
		outptr = (unsigned char *)src->str.addr;
		outtop = outptr + src->str.len;
		while (outptr < outtop)
		{
			if (',' == *outptr++)
			{
				temp_mval.mvtype = MV_STR;
				temp_mval.str.addr = (char *)outptr;
				temp_mval.str.len = INTCAST(outtop - outptr);
				s2n(&temp_mval);
				time = MV_FORCE_INTD(&temp_mval);
				if (time < 0) time = 0;
				break;
			}
		}
	}
	date = (int)MV_FORCE_INTD(src);
	date = (0 > date) ? 0 : date + 365;
	dow = ((date + 3) % 7) +1;
	for (cent = 21608 + 365, n = 3; cent < date; cent += (1461 * 25), n++)
	{
		if (n % 4)
			date++;
	}
	year = date / 1461;
	day = date - (year * 1461);
	year = year * 4 + 1840;
	if ((31 + 29 - 1) == day)
	{
		day = 29;
		month = 2;
	} else
	{
		if (day > 31 + 29 - 1)
			day--;
		month = day / 365;
		year += month;
		day -= month * 365;
		for (i = montab ; day >= *i ; day -= *i++)
			assert(i < ARRAYTOP(montab));
		month = (int)((i - montab)) + 1;
		day++;
		assert(month > 0 && month <= 12);
	}
	if ((0 == fmt->str.len) || ((1 == fmt->str.len) && ('1' == *fmt->str.addr)))
	{
		if (!TREF(zdate_form) || ((1 == TREF(zdate_form)) && (2000 > year)))
		{
			fmtptr = default1;
			fmttop = fmtptr + STR_LIT_LEN(DEFAULT1);
		} else
		{
			fmtptr = default3;
			fmttop = fmtptr + STR_LIT_LEN(DEFAULT3);
		}
	} else if ((1 == fmt->str.len) && ('2' == *fmt->str.addr))
	{
		fmtptr = default2;
		fmttop = fmtptr + STR_LIT_LEN(DEFAULT2);
	} else
	{
		fmtptr = (unsigned char *)fmt->str.addr;
		fmttop = fmtptr + fmt->str.len;
	}
	outlen = (int)(fmttop - fmtptr);
	if (outlen >= ZDATE_MAX_LEN)
		rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
	outptr = stringpool.free;
	outtop = outptr + ZDATE_MAX_LEN;
	temp_mval.mvtype = MV_STR;
	while (fmtptr < fmttop)
	{
		switch (ch = *fmtptr++)
		{
			case '/':
			case ':':
			case '.':
			case ',':
			case '-':
			case ' ':
			case '*':
			case '+':
			case ';':
				*outptr++ = ch;
				continue;
			case 'M':
				ch = *fmtptr++;
				if ('M' == ch)
				{
					n = month;
					nlen = (0 == date) ? 0 : 2;
					break;
				}
				if (('O' != ch) || ('N' != *fmtptr++))
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				if (0 == mo_str->str.len)
				{
					temp_mval.str.addr = (char *)&defmonlst[(month - 1) * 3];
					temp_mval.str.len = 3;
					nlen = (0 == date) ? 0 : -3;
				} else
				{
					UNICODE_ONLY(gtm_utf8_mode ? op_fnp1(mo_str, comma, month, &temp_mval) :
						                     op_fnzp1(mo_str, comma, month, &temp_mval));
					VMS_ONLY(op_fnzp1(mo_str, comma, month, &temp_mval, TRUE));
					nlen = (0 == date) ? 0 : -temp_mval.str.len;
					outlen += - 3 - nlen;
					if (outlen >= ZDATE_MAX_LEN)
						rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				}
				break;
			case 'D':
				ch = *fmtptr++;
				if ('D' == ch)
				{
					n = day;
					nlen = (0 == date) ? 0 : 2;
					break;
				}
				if (('A' != ch) || ('Y' != *fmtptr++))
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				if (0 == day_str->str.len)
				{
					temp_mval.str.addr = (char *)&defdaylst[(dow - 1) * 3];
					temp_mval.str.len = 3;
					nlen = (0 == date) ? 0 : -3;
				} else
				{
                                        UNICODE_ONLY(gtm_utf8_mode ? op_fnp1(day_str, comma, dow, &temp_mval) :
                                                                     op_fnzp1(day_str, comma, dow, &temp_mval));
					VMS_ONLY(op_fnzp1(day_str, comma, dow, &temp_mval, TRUE));
					nlen = (0 == date) ? 0 : -temp_mval.str.len;
					outlen += - 3 - nlen;
					if (outlen >= ZDATE_MAX_LEN)
						rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				}
				break;
			case 'Y':
				ch = *fmtptr++;
				n = year;
				if ('Y' == ch)
				{
					for (nlen = 2; (MAX_YEAR_DIGITS >=nlen) && fmtptr < fmttop; ++nlen, fmtptr++)
						if ('Y' != *fmtptr)
							break;
				} else
				{
					if (('E' != ch) || ('A' != *fmtptr++) || ('R' != *fmtptr++))
						rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
					nlen = 4;
				}
				nlen = (0 < date) ? nlen : 0;
				break;
			case '1':
				if ('2' != *fmtptr++)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				nlen = 2;
				n = time / 3600;
				n = (n + 11) % 12 + 1;
				break;
			case '2':
				if ('4' != *fmtptr++)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				nlen = 2;
				n = time / 3600;
				break;
			case '6':
				if ('0' != *fmtptr++)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				nlen = 2;
				n = time;
				n /= 60;
				n %= 60;
				break;
			case 'S':
				if ('S' != *fmtptr++)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
				nlen = 2;
				n = time % 60;
				break;
			case 'A':
				if ('M' != *fmtptr++)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);

				*outptr++ = (time < 12 * 3600) ? 'A' : 'P';
				*outptr++ = 'M';
				continue;
			default:
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
		}
		if (nlen > 0)
		{
			outptr += nlen;
			outpt1 = outptr;
			while (nlen-- > 0)
			{
				*--outpt1 = '0' + (n % 10);
				n /= 10;
			}
		} else
		{
			outpt1 = (unsigned char *)temp_mval.str.addr;
			while (nlen++ < 0)
				*outptr++ = *outpt1++;
		}
	}
	if (fmtptr > fmttop)
		rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)outptr - dst->str.addr);
	stringpool.free = outptr;
	return;
}
