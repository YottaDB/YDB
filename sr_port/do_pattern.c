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

#include "patcode.h"
#include "copy.h"
#include "min_max.h"

GBLREF uint4 *pattern_typemask;

int do_pattern(mval *str, mval *pat)
{
	unsigned short int count,total,total_max;
	unsigned short int *min,*max,*size;
	unsigned char *p,*ptop;

	bool success, attempt;
	short int	   i,j,idx;
	unsigned short int len,length,z_diff,*r,*rtop;
	unsigned short int mintmp, maxtmp, sizetmp;
	unsigned short int repeat[MAX_PATTERN_ATOMS];
	unsigned char *stridx[MAX_PATTERN_ATOMS], *patidx[MAX_PATTERN_ATOMS];
	unsigned char   *s,*pstr,*d_trans,*strbot,*strtop;
	uint4 code;

	/***************************************************************************/
	/*			      Set up information.			   */
	/***************************************************************************/

	MV_FORCE_STR(str);
	p =  (unsigned char *) pat->str.addr;
	p++;
	patidx[0] = p + 1;
	stridx[0] = (unsigned char *)str->str.addr;
	p += *p;

	GET_SHORT(count, p);
	p += sizeof(short int);
	GET_SHORT(total, p);
	p += sizeof(short int);
	GET_SHORT(total_max, p);
	p += sizeof(short int);

	length = str->str.len;
	if (total > length || total_max < length)
	{	return FALSE;
	}

	i = sizeof(short int) * count;
	min = (unsigned short int *) p;
	p += i;
	max = (unsigned short int *)p;
	size = (unsigned short int *)(p+i);

	memcpy(repeat,min,i);
	rtop = &repeat[0] + count;

	count--;
	attempt = FALSE;
	idx = 0;
	/***************************************************************************/
	/*			      Proceed to check string.			   */
	/***************************************************************************/
	for (;;)
	{

		if (total == length)
		{	/***************************************************************************/
			/*			      attempt a match.				   */
			/***************************************************************************/

			attempt = TRUE;
			s = stridx[idx];
			p = patidx[idx];
			r = &repeat[idx];
			for (; r < rtop ; r++)
			{
				code = *p++;
				/*******************************/
				/*  Meta character pat atom.   */
				/*******************************/
				if (code < PATM_STRLIT)
				{
					if (code < PATM_USRDEF) {
						/* [KMK] assert(code != 0); */
					} else {
						GET_LONG(code, (p - 1));
						p += 3;
						code &= PATM_LONGFLAGS;
					}
					for (j = 0; j < *r ;j++)
					{	if (!(code & pattern_typemask[*s++]))
						{	goto CALC;
						}
					}
				}
				/*******************************/
				/*  DFA pat atom.	       */
				/*******************************/
				else if (code == PATM_DFA)
				{
					len = *p++;
					d_trans = p;
					strbot = strtop = s;
					strtop += *r;
					while (s < strtop)
					{
						if (*d_trans < PATM_STRLIT)
						{
							if (*d_trans < PATM_USRDEF)
							{
								success = (*d_trans++ & pattern_typemask[*s]);
							}
							else
							{	int code;

								GET_LONG(code, d_trans);
								d_trans += sizeof(int4);
								code &= PATM_LONGFLAGS;
								success = code & pattern_typemask[*s];
							}
						}
						else
						{	d_trans++;
							success = (*d_trans++ == *s);
						}
						if (success)
						{
							d_trans = p + *d_trans;
							s++;
							if (*d_trans == PATM_ACS)
								break;
						}
						else
						{	d_trans++;
							if (*d_trans >= PATM_DFA)
								break;
						}
					}

					if (s < strtop)
					{	goto CALC;
					}
					else
 					{
						while(*d_trans < PATM_DFA)
						{
							if (*d_trans >= PATM_STRLIT)
							{	d_trans += sizeof(unsigned char);
							}
							d_trans += 2 * sizeof(unsigned char);
						}
						if (*d_trans != PATM_ACS)
						{	goto CALC;
						}
					}

					p += len;
				}
				/*******************************/
				/*  STRLIT pat atom.	       */
				/*******************************/
				else
				{
					len = *p++;
					if (len == 1)
					{
						for (j=0;j < *r;j++)
						{
							if (*p != *s++)
							{	goto CALC;
							}
						}
						p++;
					}
					else if (len > 0)
					{
						ptop = p+len;
						for (j=0;j < *r;j++)
						{
							pstr = p;
							while (pstr < ptop)
							{
								if (*pstr++ != *s++)
								{	goto CALC;
								}
							}
						}
						p += len;
					}
				}
				idx++;
				stridx[idx] = s;
				patidx[idx] = p;
			}
			return TRUE;
		}

		/***************************************************************************/
		/*			calculate permutations.				   */
		/***************************************************************************/

		else
		{
			attempt = FALSE;
			GET_SHORT(maxtmp, max + count);
			GET_SHORT(mintmp, min + count);
			GET_SHORT(sizetmp, size + count);
			if (repeat[count] < maxtmp)
			{
				i = j = length - total;
				z_diff = maxtmp - mintmp;
				if (sizetmp > 1)
				{	j /= sizetmp;
					i = j * sizetmp;
					z_diff *= sizetmp;
				}

				if (i > 0)
				{
					total += MIN(i,z_diff);
					repeat[count] = MIN(repeat[count]+j,maxtmp);
					if (total == length)
						continue;
				}
			}
		}

CALC:		for (j = count; ; )
		{
			GET_SHORT(mintmp, min + j);
			GET_SHORT(sizetmp, size + j);
			total -= (repeat[j] - mintmp) * sizetmp;
			repeat[j] = mintmp;
			j--;
			if (j < 0)
			{	return FALSE;
			}
			GET_SHORT(sizetmp, size + j);
			GET_SHORT(maxtmp, max + j);
			if (repeat[j] < maxtmp)
			{
				total += sizetmp;
				repeat[j]++;
				if (total <= length)
				{	if (j <= idx)
					{	idx = j;
						break;
					}
					if (!attempt)
						break;
				}
			}
		}
	}
}
