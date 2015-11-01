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
#include "op.h"
#include "mvalconv.h"

#define ZDATE_MAX_LEN 64
GBLREF spdesc stringpool;

void
op_fnzdate(mval *src,mval *fmt,mval *mo_str,mval *day_str,mval *dst)
{
	error_def(ERR_ZDATEFMT);
	int date;
	int n,nlen;
	int outlen;
	int year,month,day,time,dow,cent;
	mval temp_mval;
	unsigned char *i;
	unsigned char ch;
	static readonly unsigned char montab[] = {31,28,31,30,31,30,31,31,30,31,30,31};
	static readonly unsigned char default1[] = "MM/DD/YY";
	static readonly unsigned char default2[] = "DD-MON-YY";
	static readonly unsigned char defmonlst[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
	static readonly unsigned char defdaylst[] = "SUNMONTUEWEDTHUFRISAT";
	static readonly unsigned char comma = ',';
	unsigned char *fmtptr,*fmttop;
	unsigned char *outptr,*outtop,*outpt1;
	MV_FORCE_NUM(src);
	MV_FORCE_STR(fmt);
	MV_FORCE_STR(mo_str);
	MV_FORCE_STR(day_str);
	if (stringpool.top - stringpool.free < ZDATE_MAX_LEN)
		stp_gcol(ZDATE_MAX_LEN);
	time = 0;
	if ((src->mvtype & MV_STR) && (src->mvtype & MV_NUM_APPROX))
	{
		outptr = (unsigned char *)src->str.addr;
		outtop = outptr + src->str.len;
		while (outptr < outtop)
		{
			if (*outptr++ == ',')
			{
				temp_mval.mvtype = MV_STR;
				temp_mval.str.addr = (char *)outptr;
				temp_mval.str.len = outtop - outptr;
				s2n(&temp_mval);
				time = MV_FORCE_INT(&temp_mval);
				if (time < 0) time = 0;
				break;
			}
		}
	}
	if (fmt->str.len == 0 || (fmt->str.len == 1 && *fmt->str.addr == '1'))
	{
		fmtptr = default1;
		fmttop = fmtptr + sizeof(default1) - 1;
	} else if (fmt->str.len == 1 && *fmt->str.addr == '2')
	{
		fmtptr = default2;
		fmttop = fmtptr + sizeof(default2) - 1;
	} else
	{
		fmtptr = (unsigned char *)fmt->str.addr;
		fmttop = fmtptr + fmt->str.len;
	}
	outlen = fmttop - fmtptr;
	if (outlen >= ZDATE_MAX_LEN)
		rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
	outptr = stringpool.free;
	outtop = outptr + ZDATE_MAX_LEN;
	date = (int) MV_FORCE_INT(src);
	date += 365;
	if ((date < 0) || ((src->mvtype &MV_STR) && (MV_FORCE_INT(src) == 0)))
		date = 0;
	dow = ((date + 3) % 7) +1;
	for (cent = 21608 + 365, n = 3; cent < date; cent += (1461 * 25), n++)
	{
		if (n % 4)
		{
			date++;
		}
	}
	year = date / 1461;
	day = date - (year * 1461);
	year = year * 4 + 1840;
	if (day == 31 + 29 - 1)
	{
		day = 29;
		month = 2;
	}
	else
	{
		if (day > 31 + 29 - 1)
			day--;
		month = day / 365;
		year += month;
		day -= month * 365;
		for (i = montab ; day >= *i ; day -= *i++)
			;
		month = (i - montab) + 1;
		day++;
		assert(month > 0 && month <= 12);
	}
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
			if (ch == 'M')
			{
				n = month;
				nlen = (date == 0) ? 0 : 2;
				break;
			}
			if (ch != 'O' || *fmtptr++ != 'N')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			if (mo_str->str.len == 0)
			{
				temp_mval.str.addr = (char *)&defmonlst[(month - 1) * 3];
				temp_mval.str.len = 3;
				nlen = (date == 0) ? 0 : -3;
			} else
			{
				op_fnp1(mo_str, (int)comma, month, &temp_mval, TRUE);
				nlen = (date == 0) ? 0 : -temp_mval.str.len;
				outlen += - 3 - nlen;
				if (outlen >= ZDATE_MAX_LEN)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			}
			break;
		case 'D':
			ch = *fmtptr++;
			if (ch == 'D')
			{
				n = day;
				nlen = (date == 0) ? 0 : 2;
				break;
			}
			if (ch != 'A' || *fmtptr++ != 'Y')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			if (day_str->str.len == 0)
			{
				temp_mval.str.addr = (char *)&defdaylst[(dow - 1) * 3];
				temp_mval.str.len = 3;
				nlen = (date == 0) ? 0 : -3;
			} else
			{
				op_fnp1(day_str, (int)comma, dow, &temp_mval, TRUE);
				nlen = (date == 0) ? 0 : -temp_mval.str.len;
				outlen += - 3 - nlen;
				if (outlen >= ZDATE_MAX_LEN)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			}
			break;
		case 'Y':
			ch = *fmtptr++;
			n = year;
			if (ch == 'Y')
			{
				nlen = (date == 0) ? 0 : 2;
				break;
			}
			if (ch != 'E' || *fmtptr++ != 'A' || *fmtptr++ != 'R')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = (date == 0) ? 0 : 4;
			break;
		case '1':
			if (*fmtptr++ != '2')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time / 3600;
			n = (n + 11) % 12 + 1;
			break;
		case '2':
			if (*fmtptr++ != '4')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time / 3600;
			break;
		case '6':
			if (*fmtptr++ != '0')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time;
			n /= 60;
			n %= 60;
			break;
		case 'S':
			if (*fmtptr++ != 'S')
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time % 60;
			break;
		case 'A':
			if (*fmtptr++ != 'M')
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
	dst->str.len = (char *)outptr - dst->str.addr;
	stringpool.free = outptr;
	return;
}
