/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_limits.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

#include "ddphdr.h"
#include "ddpcom.h"
#include "dcpsubs.h"

#define SUBS_OUT(X) (is_negative  ? ('9' - (X)) : ((X) + '0'))


/********************************************
* DCPSUBS.C
*
* Convert GT.M global reference to DSM global reference
********************************************/

boolean_t dcp_g2d(unsigned char **inptradr, unsigned char **outptradr, int outbuflen, unsigned char *naked_size_p)
/*
unsigned char **inptradr;	Address of pointer to GT.M global reference to be converted
unsigned char **outptradr;	Address of pointer to DSM global reference buffer
unsigned char *naked_size_p;	Address where to store naked_size
*/

/* Upon return: the pointers pointed to by inptradr and outptradr are updated.
   If the return value is true then the conversion was completely successful.
   If false, then the conversion was completed to the extend possible.  However,
   there was a non-convertable character, such as an embedded null.
*/
{
	unsigned char	*ipt, *opt, *start_out, *opt_top;
	unsigned char	ch;
	boolean_t	succeeded;
	int 		expval, outexp, digit;
	boolean_t	is_negative;
	error_def(ERR_DDPSUBSNUL);
	error_def(ERR_GVSUBOFLOW);

	/* Work with local copies of the pointers, as there are otherwise two
	   levels of de-referencing involved
	*/
	ipt = *inptradr;
	opt = start_out = *outptradr;
	opt_top = opt + outbuflen;
	/* The 'succeeded' flag indicates whether the conversion was performed
	   with complete success.  It is initialized to 'TRUE'.  If at any time,
	   the program detects that the conversion will not be 100% accurate, the
	   variable will be set to 'FALSE'.
	*/
	succeeded = TRUE;
	/* The variable 'ch' will always contain the current character that we are
	   working on.
	*/
	/* Do global name in 7-bit format first */
	while (KEY_DELIMITER != (ch = *ipt++) && opt < opt_top)
		*opt++ = (ch << 1) + 1;
	if (KEY_DELIMITER == ch)
		*(opt - 1) &= ~1; /* the last byte of the global has bit 0 cleared, all others have bit 0 set */
	else
		rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
	while (KEY_DELIMITER != (ch = *ipt++) && opt < opt_top)
	{
		if (UCHAR_MAX < opt - start_out)
			rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
		*naked_size_p = opt - start_out;
		if (STR_SUB_PREFIX == ch || (SUBSCRIPT_STDCOL_NULL == ch &&  KEY_DELIMITER == *ipt))
		{ /* GT.M subscript is a string subscript */
			ch = *ipt++;
			if (KEY_DELIMITER == ch) /* If input subscript is null, make output subscript null */
				*opt++ = 1; /* 1 is special indicator for null string */
			else
			{ /* Non-null string is here */
				assert(STR_SUB_PREFIX == *(ipt -2));
				*opt++ = 0xFE;	/* FE is indication that non-null string follows */
				for ( ; 0 != ch && opt < opt_top; ch = *ipt++)
				{
					if (1 == ch)
					{ /* Decode GT.M style control characters */
						if (0 == (ch = (*ipt++ - 1)))
							rts_error(VARLSTCNT(1) ERR_DDPSUBSNUL);
					}
					*opt++ = ch;
				}
				if (0 != ch)
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
			}
		} else
		{ /* GT.M subscript is a numeric subscript */
			if (0x80 == ch)
			{ /* Special case: input value is zero */
				if (opt + 1 < opt_top) /* need two bytes */
				{
					*opt++ = 0x80;
					*opt++ = '0';
					ipt++;
				} else
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
			} else
			{
				is_negative = (0 == (ch & 0x80));
				if (is_negative)
					ch = ~ch;
				expval = (ch & 0x7f);
				expval -= 0x3F;		/* Remove bias */
				/* For DSM, all negative exponents are = 0x81 */
				if (-1 > expval)
					outexp = -1;
				else
					outexp = expval;
				/* 1 collation byte + 1 decimal point + expval zeros + max 18 digits + trailer if negative */
				if ((1 + 1 + (0 > expval ? -expval : expval) + 2 * (MAX_NUM_SUBSC_LEN - 1) + (is_negative ? 1 : 0))
				    > opt_top - opt) /* stricter check than necessary, the number might not be all of 18 digits */
					rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
				*opt++ = is_negative ? (0x7e - outexp) : outexp + 0x82;
				if (-1 > expval) /* Leading decimal point? */
				{ /* Yes, output decimal point and leading zeros */
					*opt++ = '.';
					for (digit = expval; 0 > ++digit; )
						*opt++ = SUBS_OUT(0);
				}
				while (0 != (ch = *ipt++) && STR_SUB_PREFIX != ch)
				{ /* Output mantissa */
					if (is_negative)
						ch = ~ch;
					ch--;
					if (-1 == expval--)
						*opt++ = '.';
					digit = (ch >> 4); /* upper nibble */
					*opt++ = SUBS_OUT(digit);
					if (-1 == expval--)
						*opt++ = '.';
					digit = (ch & 0xF); /* lower nibble */
					*opt++ = SUBS_OUT(digit);
				}
				if (STR_SUB_PREFIX == ch && KEY_DELIMITER == *ipt)	/* at end of negative subscript */
					ipt++;						/* increment past trailing zero byte */
				if (expval < -1)
				{	/* Trim possible trailing zero and possible "." */
					if (*(opt - 1) == SUBS_OUT(0))
						opt--;
					if ('.' == *(opt - 1))
						opt--;
				} else
				{ /* Output trailing zeroes, if any */
					for ( ; 0 <= expval; expval--)
						*opt++ = SUBS_OUT(0);
				}
				/* negative subscripts have a FE trailer */
				if (is_negative)
					*opt++ = 0xFE;
			}
		}
		if (opt < opt_top)
			*opt++ = 0;
		else
			rts_error(VARLSTCNT(1) ERR_GVSUBOFLOW);
	}
	/* Operation completed, clean-up and return */
	*outptradr = opt;
	*inptradr = ipt;
	return succeeded;
}
