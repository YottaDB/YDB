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

#include "mdef.h"
#include "io.h"
#include "iottdef.h"
#ifdef KEEP_zOS_EBCDIC
#include "gtm_unistd.h"
#endif

uchar_ptr_t iott_escape(uchar_ptr_t strin, uchar_ptr_t strtop, io_desc *io_ptr)
{
	unsigned char *str;
	unsigned char esc_type;
#ifdef KEEP_zOS_EBCDIC
	error_def(ERR_ASC2EBCDICCONV);
	if ((DEFAULT_CODE_SET != io_ptr->in_code_set) && ( -1 == __etoa_l((char *)strin, strtop - strin) ))
		rts_error(VARLSTCNT(4) ERR_ASC2EBCDICCONV, 2, LEN_AND_LIT("__etoa_l"));
#endif

	str = strin;
	esc_type = io_ptr->esc_state;

	while (esc_type < FINI)
	{
		switch (esc_type)
		{
		case START:
			assert(*str == ESC);
			esc_type = AFTESC;
			break;
		case AFTESC:
			switch(*str)
			{
			case ';':
			case '?':
				esc_type = SEQ2;
				break;
			case 'O':
				esc_type = SEQ4;
				break;
			case '[':
				esc_type = SEQ1;
				break;
			default:
				if (*str >= 0x30 && *str < 0x7F)
					esc_type = FINI;
				else if (*str > 0x1F && *str < 0x30)
					esc_type = SEQ2;
				else
					esc_type = BADESC;
				break;
			}
			break;
		case SEQ1:
			if (*str < 0x30 && *str > 0x1F)
				esc_type = SEQ3;
			else if (*str > 0x3F && *str < 0x7F)
				esc_type = FINI;
			else if (*str > 0x3F || *str < 0x30)
				esc_type = BADESC;
			break;
		case SEQ2:
			if (*str >= 0x30 && *str < 0x7F)
				esc_type = FINI;
			else if (*str > 0x2F || *str < 0x20)
				esc_type = BADESC;
			break;
		case SEQ3:
			if (*str > 0x3F && *str < 0x7F)
				esc_type = FINI;
			else if (*str > 0x2F || *str < 0x20)
				esc_type = BADESC;
		case SEQ4:
			if (*str >= 0x40 && *str < 0x7F)
				esc_type = FINI;
			else if (*str > 0x2F || *str < 0x20)
				esc_type = BADESC;
			break;
		default:
			GTMASSERT;
		}
		if (esc_type == BADESC || ++str >= strtop)
			break;
	}
	io_ptr->esc_state = esc_type;
#ifdef KEEP_zOS_EBCDIC
	if ((DEFAULT_CODE_SET != io_ptr->in_code_set) && ( -1 == __atoe_l((char *)strin, strtop - strin) ))
		rts_error(VARLSTCNT(4) ERR_ASC2EBCDICCONV, 2, LEN_AND_LIT("__atoe_l"));
#endif
	return str;
}
