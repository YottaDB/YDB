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
#include "io.h"
#include "iottdef.h"
#include "dollarx.h"
#include "patcode.h"

GBLREF uint4 *pattern_typemask;

void dollarx(io_desc *io_ptr, unsigned char *str, unsigned char *strtop)
{
	unsigned char	*str1;
	int4		esc_level;

	if (io_ptr->write_filter)
	{
		esc_level = (io_ptr->write_filter & ESC_MASK);
		while (str < strtop)
		{
			if (START != io_ptr->esc_state)
			{
				assert (esc_level);
				str1 = iott_escape(str, strtop, io_ptr);
				str = str1;
				if ((FINI == io_ptr->esc_state) || ( BADESC == io_ptr->esc_state))
					io_ptr->esc_state = START;
				continue;
			}
			if (io_ptr->write_filter & CHAR_FILTER)
			{
				switch(*str)
				{
					case NATIVE_LF:
						io_ptr->dollar.y++;
						if (io_ptr->length)
							io_ptr->dollar.y %= io_ptr->length;
						str++;
						break;
					case NATIVE_CR:
						io_ptr->dollar.x = 0;
						str++;
						break;
					case NATIVE_BS:
						if (io_ptr->dollar.x > 0)
							io_ptr->dollar.x--;
						str++;
						break;
					case NATIVE_FF:
						io_ptr->dollar.x = io_ptr->dollar.y = 0;
						str++;
						break;
					case NATIVE_ESC:
						if (esc_level)
						{
							str1 = iott_escape(str, strtop, io_ptr);
							str = str1;
							if ((FINI == io_ptr->esc_state) || ( BADESC == io_ptr->esc_state))
								io_ptr->esc_state = START;
							continue;
						}
					/*** Caution: FALL THROUGH ***/
					default:
						if (!(pattern_typemask[*str] & PATM_C))
							io_ptr->dollar.x++;
						str++;
						break;
				}
			} else if (NATIVE_ESC == *str)
			{
				assert(esc_level);
				str1 = iott_escape(str, strtop, io_ptr);
				str = str1;
				if ((FINI == io_ptr->esc_state) || (BADESC == io_ptr->esc_state))
					io_ptr->esc_state = START;
			} else
			{
				io_ptr->dollar.x++;
				str++;
			}
		}
	} else
		io_ptr->dollar.x += (strtop - str);
	if (io_ptr->dollar.x > io_ptr->width && io_ptr->wrap)
	{
		io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}
}
