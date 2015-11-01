/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "subscript.h"
#include "str2gvargs.h"

static mval subsc[MAX_GVSUBSCRIPTS]; 	/* At return, op_gvargs elements will be pointing to elements of this array, hence static */
static mstr subsc_buffer = {0, NULL}; 	/* Buffer space (subsc_buffer.addr) will be allocated on the first call.
					 * Buffer space to hold string mvals in subsc; we don't want to use stringpool because
					 * this module is called from both DDPGVUSR and GTMSHR, and stringpool is not set up in
					 * DDPGVUSR. In addition, if we use stringpool, we might have to garbage collect, which
					 * means pulling in all GBLDEFs and functions referenced in stp_gcol into DDPGVUSR.
					 * That will be a link nightmare! */

boolean_t str2gvargs(char *cp, int len, gvargs_t *op_gvargs)
{ /* IMPORTANT : op_gvargs will point to static area which gets overwritten by the next call to this function. Callers should
     make a copy of op_gvargs if necessary */

	char		*p1, *p2, *c_top, *c_ref, ch, *subsc_ptr;
	int		count;
	mval		*spt;
	int 		dollarc_val;
	boolean_t	naked, concat, dot_seen;
	error_def(ERR_NOTGBL);
	error_def(ERR_GVINVALID);
	error_def(ERR_LPARENREQD);
	error_def(ERR_NUMUNXEOR);
	error_def(ERR_STRUNXEOR);
	error_def(ERR_DLRCUNXEOR);
	error_def(ERR_DLRCTOOBIG);
	error_def(ERR_EORNOTFND);
	error_def(ERR_RPARENREQD);

	assert(sizeof(op_gvargs->count) == sizeof(op_gvargs->args[0]));
	naked = FALSE;
	concat = FALSE;
	c_ref = cp;
	c_top = cp + len;
	assert(0 < len); /* why is our code calling with "" string? */
	if (len > subsc_buffer.len)
	{
		if (NULL != subsc_buffer.addr)
			free(subsc_buffer.addr);
		subsc_buffer.len = ((MAX_ZWR_INFLATION * MAX_KEY_SZ) < len ? len : (MAX_ZWR_INFLATION * MAX_KEY_SZ));
		subsc_buffer.addr = malloc(subsc_buffer.len);
	}
	spt = subsc;
	count = 0;
	if (0 >= len || '^' != *cp++)
		rts_error(VARLSTCNT(4) ERR_NOTGBL, 2, (len > 0) ? len : 0, c_ref);
	spt->mvtype = MV_STR;
	spt->str.addr = cp;
	ch = *cp;
	if ('(' == ch)
	{
		spt->str.len = cp - spt->str.addr - 1;
		naked = TRUE;
	} else
	{
		cp++;
		if (!ISALPHA(ch) && '%' != ch)
			rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, len, c_ref);
		for ( ; cp < c_top && *cp != '('; )
		{
			ch = *cp++;
			if (!ISALPHA(ch) && !ISDIGIT(ch))
				rts_error(VARLSTCNT(4) ERR_GVINVALID, 2, len, c_ref);
		}
		spt->str.len = cp - spt->str.addr;
		op_gvargs->args[count] = spt;
		count++;
		spt++;
	}
	subsc_ptr = subsc_buffer.addr;
	if (cp < c_top)
	{
		if ('(' != *cp++)
			rts_error(VARLSTCNT(4) ERR_LPARENREQD, 2, len, c_ref);
		for (; ;)
		{
			spt->mvtype = MV_STR;
			ch = *cp;
			if ('\"' == ch)
			{
				if (!concat)
				{
					spt->str.addr = subsc_ptr;
					p1 = subsc_ptr;
				} else
					p2 = p1;
				++cp;
				for (; ;)
				{
					if (cp == c_top)
						rts_error(VARLSTCNT(4) ERR_STRUNXEOR, 2, len, c_ref);
					if ('\"' == *cp)
						if ('\"' != *++cp)
							break;
					*p1++ = *cp++;
				}
				if (!concat)
				{
					spt->str.len = p1 - spt->str.addr;
					subsc_ptr = p1;
				} else
				{
					spt->str.len += p1 - p2;
					subsc_ptr += p1 - p2;
				}
				if ('_' == *cp)
				{
					cp++;
					concat = TRUE;
					continue;
				}
			} else if ('$' == ch)
			{
				if (3 > c_top - ++cp || 'C' != toupper(*cp) || '(' != *(++cp))
					rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
				cp++;
				for (; ;)
				{
					dollarc_val = 0;
					if (!ISDIGIT(*cp))
						rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					do
					{
						dollarc_val *= 10;
						dollarc_val += *cp - '0';
						if (dollarc_val > 255)
							rts_error(VARLSTCNT(4) ERR_DLRCTOOBIG, 2, len, c_ref);
						if (++cp == c_top)
							rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					} while (ISDIGIT(*cp));
					if (!concat)
					{
						spt->str.addr = subsc_ptr;
						*spt->str.addr = dollarc_val;
						spt->str.len = 1;
						subsc_ptr++;
						p1 = subsc_ptr;
					} else
					{
						*p1++ = dollarc_val;
						spt->str.len++;
						subsc_ptr++;
					}
					if (',' == *cp)
					{
						concat = TRUE;
						if (++cp == c_top)
							rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
						continue;
					}
					if (')' != *cp)
						rts_error(VARLSTCNT(4) ERR_DLRCUNXEOR, 2, len, c_ref);
					break;
				}
				cp++;
				if ('_' == *cp)
				{
					cp++;
					concat = TRUE;
					continue;
				}
			} else
			{
				dot_seen = FALSE;
				if (!ISDIGIT(ch) && '.' != ch && '-' != ch && '+' != ch)
					rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, len, c_ref);
				if (!concat)
				{
					spt->str.addr = subsc_ptr;
					p1 = subsc_ptr;
				} else
					p2 = p1;
				*p1++ = *cp++;
				for (; ;)
				{
					if (cp == c_top)
						rts_error(VARLSTCNT(4) ERR_NUMUNXEOR, 2, len, c_ref);
					if (!ISDIGIT(*cp))
					{
						if ('.' != *cp)
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
					subsc_ptr = p1;
				} else
				{
					spt->str.len += p1 - p2;
					subsc_ptr += p1 - p2;
				}
				if ('_' == *cp)
				{
					cp++;
					concat = TRUE;
					continue;
				}
			}
			op_gvargs->args[count] = spt;
			count++;
			spt++;
			if (',' != *cp)
				break;
			concat = FALSE;
			cp++;
		}
		if (')' != *cp++)
			rts_error(VARLSTCNT(4) ERR_RPARENREQD, 2, len, c_ref);
		if (cp < c_top)
			rts_error(VARLSTCNT(4) ERR_EORNOTFND, 2, len, c_ref);
	}
	op_gvargs->count = count;
	return naked;
}
