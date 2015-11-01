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

GBLDEF unsigned char write_filter;

void dollarx(io_desc *io_ptr, unsigned char *str, unsigned char *strtop)
{
	unsigned char esc_level, *str1;

	if (write_filter)
	{
		esc_level = (write_filter & ESC_MASK);
		while (str < strtop)
		{
			if (io_ptr->esc_state != START)
			{
				assert (esc_level);
				str1 = iott_escape(str, strtop, io_ptr);
				str = str1;
				if (io_ptr->esc_state == FINI || io_ptr->esc_state == BADESC)
					io_ptr->esc_state = START;
				continue;
			}
			if (write_filter & CHAR_FILTER)
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
							if (io_ptr->esc_state == FINI || io_ptr->esc_state == BADESC)
								io_ptr->esc_state = START;
							continue;
						}
					/*** Caution: FALL THROUGH ***/
					default:
						io_ptr->dollar.x++;
						str++;
						break;
				}
			}
			else if (*str == NATIVE_ESC)
			{
				assert(esc_level);
				str1 = iott_escape(str, strtop, io_ptr);
				str = str1;
				if (io_ptr->esc_state == FINI || io_ptr->esc_state == BADESC)
					io_ptr->esc_state = START;
			}
			else
			{
				io_ptr->dollar.x++;
				str++;
			}
		}
	}
	else
		io_ptr->dollar.x += (strtop - str);

	if (io_ptr->dollar.x > io_ptr->width && io_ptr->wrap)
	{
		io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}
}
