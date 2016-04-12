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

#include "xfer_enum.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"

GBLREF	spdesc		stringpool;
GBLREF	boolean_t	gtm_utf8_mode;

error_def(ERR_ZDATEFMT);
error_def(ERR_ZDATEBADDATE);
error_def(ERR_ZDATEBADTIME);


#define	ADJUST_TO_1900		(COMMON_LEAP_CYCLE - 1)
#define	BASE_YEAR		1840					/* from the M standard */
#define	COMMON_LEAP_CYCLE	4
#define	DAYS_BASE_TO_1900	(((1900 - BASE_YEAR) / COMMON_LEAP_CYCLE * DAYS_IN_FOUR_YEARS) - 1 + DAYS_BEFORE_LEAP)
#define	DAYS_BEFORE_LEAP	(MAX_DAYS_IN_MONTH + MIN_DAYS_IN_MONTH)
#define	DAYS_IN_CENTURY		(DAYS_IN_FOUR_YEARS * 25)		/* right for leap century but on high for others */
#define	DAYS_IN_FOUR_YEARS	((DAYS_MOST_YEARS * COMMON_LEAP_CYCLE) + 1)
#define	DAYS_IN_WEEK		7
#define	DAYS_MOST_YEARS		365
#define	DEFAULT1		"MM/DD/YY"
#define	DEFAULT2		"DD-MON-YY"
#define	DEFAULT3		"MM/DD/YEAR"
#define	HOURS_PER_AM_OR_PM	12
#define	LEN_OF_3_CHAR_ABBREV	3
#define	MAX_DATE		364570088				/* 31-Dec-999999 */
#define	MAX_DAYS_IN_MONTH	31
#define	MAX_TIME		((SECONDS_PER_HOUR * 24) - 1)		/* a second before midnight */
#define	MAX_YEAR		999999
#define	MAX_YEAR_DIGITS		6
#define	MIN_DATE		(-DAYS_MOST_YEARS)			/* 01-JAN-1840 */
#define	MIN_DAYS_IN_MONTH	28
#define	MINUTES_PER_HOUR	60
#define	MONTHS_IN_YEAR		12
#define	PIVOT_MILLENIUM		2000
#define	SECONDS_PER_HOUR	(SECONDS_PER_MINUTE * MINUTES_PER_HOUR)
#define	SECONDS_PER_MINUTE	60
#define	ZDATE_MAX_LEN		64

void op_fnzdate(mval *src, mval *fmt, mval *mo_str, mval *day_str, mval *dst)
{
	unsigned char 	ch, *fmtptr, *fmttop, *i, *outptr, *outtop, *outpt1;
	int 		cent, day, dow, month, nlen, outlen, time, year;
	unsigned int	n;
	mval 		temp_mval;

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
	outlen = src->str.len;
	if ((src->mvtype & MV_STR) && (src->mvtype & MV_NUM_APPROX))
	{
		for (outptr = (unsigned char *)src->str.addr, outtop = outptr + outlen; outptr < outtop; )
		{
			if (',' == *outptr++)
			{
				outlen = outptr - (unsigned char *)src->str.addr - 1;
				temp_mval.mvtype = MV_STR;
				temp_mval.str.addr = (char *)outptr;
				temp_mval.str.len = INTCAST(outtop - outptr);
				s2n(&temp_mval);
				time = MV_FORCE_INTD(&temp_mval);
				if ((0 > time) || (MAX_TIME < time))
					rts_error(VARLSTCNT(4) ERR_ZDATEBADTIME, 2, temp_mval.str.len, temp_mval.str.addr);
				break;
			}
		}
	}
	day = (int)MV_FORCE_INTD(src);
	if ((MAX_DATE < day) || (MIN_DATE > day))
	{
		MV_FORCE_STR(src);
		rts_error(VARLSTCNT(4) ERR_ZDATEBADDATE, 2, outlen, src->str.addr);
	}
	day += DAYS_MOST_YEARS;
	dow = ((day + ADJUST_TO_1900) % DAYS_IN_WEEK) + 1;
	for (cent = DAYS_BASE_TO_1900, n = ADJUST_TO_1900; cent < day; cent += DAYS_IN_CENTURY, n++)
			day += (0 < (n % COMMON_LEAP_CYCLE));
	year = day / DAYS_IN_FOUR_YEARS;
	day = day - (year * DAYS_IN_FOUR_YEARS);
	year = (year * COMMON_LEAP_CYCLE) + BASE_YEAR;
	if (DAYS_BEFORE_LEAP == day)
	{
		day = MIN_DAYS_IN_MONTH + 1;
		month = 2;
	} else
	{
		if (DAYS_BEFORE_LEAP < day)
			day--;
		month = day / DAYS_MOST_YEARS;
		year += month;
		day -= (month * DAYS_MOST_YEARS);
		for (i = montab; day >= *i; day -= *i++)
			;
		month = (int)((i - montab)) + 1;
		day++;
		assert((0 < month) && (MONTHS_IN_YEAR >= month));
	}
	if ((0 == fmt->str.len) || ((1 == fmt->str.len) && ('1' == *fmt->str.addr)))
	{
		if (!TREF(zdate_form) || ((1 == TREF(zdate_form)) && (PIVOT_MILLENIUM > year)))
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
	assert(0 <= time);
	nlen = 0;
	while (fmtptr < fmttop)
	{
		switch (ch = *fmtptr++)		/* NOTE assignment */
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
				nlen = 2;
				break;
			}
			if (('O' != ch) || ('N' != *fmtptr++))
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			if (0 == mo_str->str.len)
			{
				temp_mval.str.addr = (char *)&defmonlst[(month - 1) * LEN_OF_3_CHAR_ABBREV];
				temp_mval.str.len = LEN_OF_3_CHAR_ABBREV;
				nlen = -LEN_OF_3_CHAR_ABBREV;
			} else
			{
				UNICODE_ONLY(gtm_utf8_mode ? op_fnp1(mo_str, comma, month, &temp_mval) :
					                     op_fnzp1(mo_str, comma, month, &temp_mval));
				VMS_ONLY(op_fnzp1(mo_str, comma, month, &temp_mval, TRUE));
				nlen = -temp_mval.str.len;
				outlen += - LEN_OF_3_CHAR_ABBREV - nlen;
				if (outlen >= ZDATE_MAX_LEN)
					rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			}
			break;
		case 'D':
			ch = *fmtptr++;
			if ('D' == ch)
			{
				n = day;
				nlen = 2;
				break;
			}
			if (('A' != ch) || ('Y' != *fmtptr++))
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			if (0 == day_str->str.len)
			{
				temp_mval.str.addr = (char *)&defdaylst[(dow - 1) * LEN_OF_3_CHAR_ABBREV];
				temp_mval.str.len = LEN_OF_3_CHAR_ABBREV;
				nlen = -LEN_OF_3_CHAR_ABBREV;
			} else
			{
				UNICODE_ONLY(gtm_utf8_mode ? op_fnp1(day_str, comma, dow, &temp_mval)
							   : op_fnzp1(day_str, comma, dow, &temp_mval));
				VMS_ONLY(op_fnzp1(day_str, comma, dow, &temp_mval, TRUE));
				nlen = -temp_mval.str.len;
				outlen += - LEN_OF_3_CHAR_ABBREV - nlen;
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
			break;
		case '1':
			if ('2' != *fmtptr++)
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time / SECONDS_PER_HOUR;
			n = ((n + HOURS_PER_AM_OR_PM - 1) % HOURS_PER_AM_OR_PM) + 1;
			break;
		case '2':
			if ('4' != *fmtptr++)
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time / SECONDS_PER_HOUR;
			break;
		case '6':
			if ('0' != *fmtptr++)
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time;
			n /= MINUTES_PER_HOUR;
			n %= MINUTES_PER_HOUR;
			break;
		case 'S':
			if ('S' != *fmtptr++)
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			nlen = 2;
			n = time % SECONDS_PER_MINUTE;
			break;
		case 'A':
			if ('M' != *fmtptr++)
				rts_error(VARLSTCNT(1) ERR_ZDATEFMT);
			*outptr++ = (time < (HOURS_PER_AM_OR_PM * SECONDS_PER_HOUR)) ? 'A' : 'P';
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
