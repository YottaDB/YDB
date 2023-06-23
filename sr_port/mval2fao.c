/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * This routine takes an arbitrary list of mvals and converts it to an array of fao parameters, for use in a subsequent output
 * call. The parameters list is determined by the fao control string, message, with limitations on the number of parameters allowed,
 * and the buffer space available for certain fao directive formats. The caller should call va_end.
 */

#include "mdef.h"

#include "gtm_string.h"
#include <stdarg.h>

#include "fao_parm.h"
#include "mvalconv.h"
#include "mval2fao.h"

int mval2fao(
	char		*message,		/* Text of message in fao format. */
	va_list		pfao,			/* Argument list of caller. */
	UINTPTR_T	*outparm,		/* Array of resulting fao parameters. */
	int		mcount, int fcount,	/* mvalcount and faocount. */
	char		*bufbase, char *buftop)	/* Buffer space for strings and indirect arguments. */
{
	char		*buf;
	int		n, parmcnt, indir_index;
	gtm_int64_t	num;
	mval		*fao;

	fao = va_arg(pfao, mval *);
	parmcnt = 0;
	/* Storage of indirect arguments is at the end of the array. */
	indir_index = NUM_OF_FAO_SLOTS;
	buf = bufbase;
	for ( ; mcount && (parmcnt < fcount); )
	{
		MV_FORCE_DEFINED(fao);
		while (*message != '!')
			DBG_ASSERT(*message) message++;
		/* A length for the fao parameter (consisting of digits). */
		for (n = 0; (*++message > 47) && (*message < 58); n++)
			;
		switch (*message++)
		{
			case '/':
			case '_':
			case '^':
			case '!':
				break;
/* String */		case 'A':
				MV_FORCE_STR(fao);
				switch (*message++)
				{
/* ASCII counted string */		case 'C':
						/* Since the maximum length that could be encoded in one byte is 255, the total
						 * length of the mval can be at most 256 (the byte containing the count plus 255
						 * bytes of the actual string.
						 */
						if (((fao)->str.len > 256) || ((fao)->str.len < 0))
							return -1;
						if (buf + (fao)->str.len + 1 >= buftop)
							return -1;
						outparm[parmcnt++] = (UINTPTR_T)buf;
						*buf++ = (fao)->str.len;
						memcpy(buf, (fao)->str.addr, (fao)->str.len);
						buf += (fao)->str.len;
						break;
/* len,addr string, '.' filled */	case 'F':
/* len,addr string */			case 'D':
						if (parmcnt + 2 > fcount)
							return parmcnt;
						outparm[parmcnt++] = (unsigned int)(fao)->str.len;
						outparm[parmcnt++] = (UINTPTR_T)(fao)->str.addr;
						break;
/* ASCII string descriptor */		case 'S':
						if (buf + SIZEOF(desc_struct) >= buftop)
							return -1;
						((desc_struct *)buf)->len = (fao)->str.len;
						((desc_struct *)buf)->addr = (fao)->str.addr;
						outparm[parmcnt++] = (UINTPTR_T)buf;
						buf += SIZEOF(desc_struct);
						break;
/* NULL-terminated string */		case 'Z':
						if (buf + (fao)->str.len + 1 >= buftop)
							return -1;
						outparm[parmcnt++] = (UINTPTR_T)buf;
						memcpy(buf, (fao)->str.addr, (fao)->str.len);
						buf += (fao)->str.len;
						*buf++ = '\0';
						break;
					default:
						return -1;
				}
				fao = va_arg(pfao, mval *);
				mcount--;
				break;
/* Octal number */	case 'O':
/* Hex number */	case 'X':
/* Signed number */	case 'S':
/* Zero-filled num */	case 'Z':
/* Unsigned num */	case 'U':
				/* Extracting the value as an integer, since util_format will take care of further casting. */
				num = mval2i8(fao);
				switch (*message++)
				{
					case 'B':
					case 'W':
					case 'L':
					case 'J':
						outparm[parmcnt++] = (UINTPTR_T)num;
						break;
					default:
						return -1;
				}
				fao = va_arg(pfao, mval *);
				mcount--;
				break;
/* Indirect argument */	case '@':
				switch (*message++)
				{
/* Zero-filled num */			case 'Z':
/* Unsigned num */			case 'U':
					case 'X':
						num = mval2i8(fao);
						switch (*message++)
						{
							case 'J':
							case 'Q':
								indir_index -= GTM64_ONLY(1) NON_GTM64_ONLY(2);
								*(gtm_int64_t *)(outparm + indir_index) = num;
								outparm[parmcnt++] = (UINTPTR_T)(&outparm[indir_index]);
								break;
							default:
								return -1;
						}
						fao = va_arg(pfao, mval *);
						mcount -= 1;
						/* Either we already processed all the arguments, or we should still have room in
						 * our array for another indirect argument.
						 */
						assert((0 == mcount) || (parmcnt + GTM64_ONLY(1) NON_GTM64_ONLY(2) < indir_index));
						break;
					default:
						return -1;
				}
				break;
			default:
				return -1;
		}
	}
	return parmcnt;
}
