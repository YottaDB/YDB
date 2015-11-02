/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *-----------------------------------------------------------------------------
	This routine takes an arbitrary list of mvals and converts it to an
	array of fao parameters, for use in a subsequent output call.

	The parameters list is determined by the fao control string, message,
	with limitations on the number of parameters allowed, and the
	buffer space available for certain fao directive formats.

	The caller should call va_end.
 *-----------------------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"
#include <stdarg.h>

#include "fao_parm.h"
#include "mvalconv.h"
#include "mval2fao.h"

int mval2fao(
	char		*message,		/* text of message in fao format */
	va_list		pfao,			/* argument list of caller */
	UINTPTR_T	*outparm,		/* array of resulting fao parameters */
	int		mcount, int fcount,	/* mvalcount and faocount */
	char		*bufbase, char *buftop)	/* buffer space for !AC and !AS */
{
	char		*buf;
	int		i, parmcnt, num;
	mval		*fao;

	fao = va_arg(pfao, mval *);
	parmcnt = 0;
	buf = bufbase;
	for ( ; mcount && parmcnt < fcount; )
	{
		MV_FORCE_DEFINED(fao);
		while (*message != '!')
			message++;
		for (i=0;(*++message > 47) && (*message < 58);i++)		/* a length for the fao parameter */
			;
		switch (*message++)
		{
			case '/':
			case '_':
			case '^':
			case '!':	break;
			case 'A':	MV_FORCE_STR(fao);
					switch(*message++)
					{
/* ascii counted string */			case 'C':
								if ((fao)->str.len > 256 || (fao)->str.len < 0)
									return -1;
								if (buf + (fao)->str.len + 1 >= buftop)
									return -1;
								*buf++ = (fao)->str.len;
								memcpy(buf, (fao)->str.addr, (fao)->str.len);
								buf += (fao)->str.len;
								break;
/* len,addr string, '.' filled */		case 'F':
/* len,addr string */				case 'D':
								if (parmcnt + 2 > fcount)
									return parmcnt;
								outparm[parmcnt++] = (unsigned int)(fao)->str.len;
								outparm[parmcnt++] = (UINTPTR_T)(fao)->str.addr;
								break;
/* ascii string descriptor */			case 'S':
								if (buf + SIZEOF(desc_struct) >= buftop)
									return -1;
								((desc_struct *)buf)->len = (fao)->str.len;
								((desc_struct *)buf)->addr = (fao)->str.addr;
								outparm[parmcnt++] = (UINTPTR_T)buf;
								buf += SIZEOF(desc_struct);
								break;
						default:	return -1;
					}
					fao = va_arg(pfao, mval *);
					mcount--;
					break;
/* octal number */	case 'O':
/* hex number */	case 'X':
/* signed number */	case 'S':
					num = MV_FORCE_INT(fao);
					switch(*message++)
					{
						case 'B':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						case 'W':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						case 'L':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						default:	return -1;
					}
					fao = va_arg(pfao, mval *);
					mcount--;
					break;
/* zero filled num */	case 'Z':
/* unsigned num */	case 'U':
					num = MV_FORCE_INT(fao);
					switch(*message++)
					{
						case 'B':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						case 'W':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						case 'L':	outparm[parmcnt++] = (UINTPTR_T)num;
								break;
						default:	return -1;
					}
					fao = va_arg(pfao, mval *);
					mcount--;
					break;
			default:	return -1;
		}
	}
	return parmcnt;
}
