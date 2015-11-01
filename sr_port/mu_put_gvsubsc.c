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
#include "subscript.h"
#include "op.h"
#include "mupip_put_gvsubsc.h"
#include "callg.h"

GBLREF spdesc stringpool;

static	mval	subsc[MAX_GVSUBSCRIPTS];
struct
{
	int4	count;		/* caveat: this should be the same size as a pointer */
	mval	*args[MAX_GVSUBSCRIPTS + 1];
} op_gvargs;

void	mupip_put_gvsubsc (char *cp, int len, boolean_t call_gvfunc)
{
	error_def(ERR_NOTGBL);
	error_def(ERR_GVINVALID);
	error_def(ERR_LPARENREQD);
	error_def(ERR_NUMUNXEOR);
	error_def(ERR_STRUNXEOR);
	error_def(ERR_DLRCUNXEOR);
	error_def(ERR_DLRCTOOBIG);
	error_def(ERR_EORNOTFND);
	error_def(ERR_RPARENREQD);
	char		*p1, *p2, *c_top, *c_ref, ch;
	int		count;
	mval		*spt;
	unsigned short	dollarc_val, i;
	bool		naked, concat, dot_seen;

	assert (sizeof(op_gvargs.count) == sizeof(op_gvargs.args[0]));
	naked = FALSE;
	concat = FALSE;
	c_ref = cp;
	c_top = cp + len;
	spt = subsc;
	count = 0;
	if (*cp++ != '^')
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, len, c_ref);
	spt->mvtype = MV_STR;
	spt->str.addr = cp;
	ch = *cp;
	if ( ch == '(' )
		naked = TRUE;
	else
	{
		cp++;
		if ((ch < 'A' || ch > 'Z')  &&  ch != '%'  &&  (ch < 'a' || ch > 'z'))
			rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, len, c_ref);
		for (  ;  cp < c_top  &&  *cp != '(' ;  )
		{
			ch = *cp++;
			if ((ch < 'A' || ch > 'Z')  &&  (ch < 'a' || ch > 'z')  &&  (ch < '0' || ch > '9'))
				rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, len, c_ref);
		}
	}
	spt->str.len = cp - spt->str.addr - naked;
	if (!naked)
	{
		op_gvargs.args[count] = spt;
		count++;
		spt++;
	}
	if (cp < c_top)
	{
		if (*cp++ != '(')
			rts_error(VARLSTCNT(4) ERR_LPARENREQD, 2, len, c_ref);
		for (  ;  ;  )
		{
			spt->mvtype = MV_STR;
			ch = *cp;
			if (ch == '\"')
			{
				if (!concat)
				{
					spt->str.addr = (char*)stringpool.free;
					p1 = (char*)stringpool.free;
				}
				else
					p2 = p1;
				++cp;
				for (  ;  ;  )
				{
					if (cp == c_top)
						rts_error(VARLSTCNT(4) ERR_STRUNXEOR, 2, len, c_ref);
					if (*cp == '\"')
						if (*++cp != '\"')
							break;
					*p1++ = *cp++;
				}
				if (!concat)
				{
					spt->str.len = p1 - spt->str.addr;
					stringpool.free = (unsigned char*)p1;
				}else
				{
					spt->str.len += p1 - p2;
					stringpool.free += p1 - p2;
				}
				if (*cp == '_')
				{
					cp++;
					concat = TRUE;
					continue;
				}
			}
			else if (ch == '$')
			{
				if (++cp == c_top)
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				if (*cp != 'C' && *cp != 'c')
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				if (++cp == c_top)
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				if (*cp != '(')
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				if (++cp == c_top)
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				while (TRUE)
				{
					dollarc_val = 0;
					if (*cp < '0'  ||  *cp > '9')
						rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					do
					{
						dollarc_val *= 10;
						dollarc_val += *cp - '0';
						if (dollarc_val > 255)
							rts_error(VARLSTCNT(4) ERR_DLRCTOOBIG, 2, len, c_ref);
						if (++cp == c_top)
							rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					} while (*cp >= '0'  &&  *cp <= '9');
					if (!concat)
					{
						spt->str.addr = (char*)stringpool.free;
						*spt->str.addr = dollarc_val;
						spt->str.len = 1;
						p1 = (char*)(++stringpool.free);
					}
					else
					{
						*p1++ = dollarc_val;
						spt->str.len++;
						stringpool.free++;
					}
					if (*cp == ',')
					{
						concat = TRUE;
						if (++cp == c_top)
							rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
						continue;
					}
					if (*cp != ')')
						rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					break;
				}
				cp++;
				if (*cp == '_')
				{
					cp++;
					concat = TRUE;
					continue;
				}
			}
			else
			{
				dot_seen = FALSE;
				if ((ch > '9' || ch < '0')  &&  ch != '.'  &&  ch != '-'  &&  ch != '+')
					rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, len, c_ref);
				if (!concat)
				{
					spt->str.addr = (char*)stringpool.free;
					p1 = (char*)stringpool.free;
				}
				else
					p2 = p1;
				*p1++ = *cp++;
				for (  ;  ;  )
				{
					if (cp == c_top)
						rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, len, c_ref);
					if (*cp > '9'  ||  *cp < '0')
					{
						if (*cp != '.')
							break;
						else if (!dot_seen)
							dot_seen = TRUE;
						else
							rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, len, c_ref);
					}
					*p1++ = *cp++;
				}
				if (!concat)
				{
					spt->str.len = p1 - spt->str.addr;
					stringpool.free = (unsigned char*)p1;
				}
				else
				{
					spt->str.len += p1 - p2;
					stringpool.free += p1 - p2;
				}
				if (*cp == '_')
				{
					cp++;
					concat = TRUE;
					continue;
				}
			}
			op_gvargs.args[count] = spt;
			count++;
			spt++;
			if (*cp != ',')
				break;
			concat = FALSE;
			cp++;
		}
		if (*cp++ != ')')
			rts_error(VARLSTCNT(4) ERR_RPARENREQD, 2, len, c_ref);
		if (cp < c_top)
			rts_error(VARLSTCNT(4) ERR_EORNOTFND, 2, len, c_ref);
	}
	op_gvargs.count = count;
	if (TRUE == call_gvfunc)
		callg((int(*)())(naked ? op_gvnaked : op_gvname), &op_gvargs);
	return;
}
