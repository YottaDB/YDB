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

#include <wctype.h>

#include "io.h"
#include "iottdef.h"
#include "dollarx.h"
#include "patcode.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"	/* needed by *TYPEMASK* macros defined in gtm_utf8.h */
#include "gtm_utf8.h"

LITREF	UChar32	u32_line_term[];
#endif

GBLREF	uint4		*pattern_typemask;
GBLREF	boolean_t	gtm_utf8_mode;

void dollarx(io_desc *io_ptr, unsigned char *str, unsigned char *strtop)
{
	unsigned char	*str1, *strnext, *strstart, *strcursor, *strprev;
	int4		esc_level, char_width, total;
	boolean_t	utf8_term, utf8_active, utf8_crlast = FALSE;
	wint_t		curr_char;

	utf8_active = (gtm_utf8_mode UNICODE_ONLY(&& CHSET_M != io_ptr->ochset)) ? TRUE : FALSE;
	utf8_term = (utf8_active && tt == io_ptr->type) ? TRUE : FALSE;
	strstart = strcursor = str;
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
			if (!utf8_active)
			{
				curr_char = *str;
				strnext = str + 1;
			}
#ifdef UNICODE_SUPPORTED
			else
				strnext = UTF8_MBTOWC(str, strtop, curr_char);
#endif
			if (io_ptr->write_filter & CHAR_FILTER)
			{
				switch(curr_char)
				{
					case NATIVE_LF:
						if (!utf8_crlast)
						{	/* otherwise CR case will have handled */
							io_ptr->dollar.y++;
							if (io_ptr->length)
								io_ptr->dollar.y %= io_ptr->length;
						} else
							utf8_crlast = FALSE;
						str = strnext;
						break;
					case NATIVE_CR:
						io_ptr->dollar.x = 0;
						if (utf8_active && gtmsocket != io_ptr->type)
						{	/* CR implies LF for Unicode except for socket which recongizes
							   only NATIVE_LF as a terminator unicode or not.
							*/
							utf8_crlast = TRUE;
							io_ptr->dollar.y++;
							if (io_ptr->length)
								io_ptr->dollar.y %= io_ptr->length;
						}
						str = strstart = strcursor = strnext;
						break;
					case NATIVE_BS:
						/* if bs at beginning of string but x > 0 need image of line */
						if (io_ptr->dollar.x > 0)
#ifdef UNICODE_SUPPORTED
							if (utf8_term)
							{
								/* get previous character relative to strcursor and back it up */
								if (strstart < strcursor)
								{
									for ( ; strstart < strcursor; strcursor = strprev)
									{
										UTF8_LEADING_BYTE((strcursor - 1), strstart,
											strprev);
										UTF8_MBTOWC(strprev, strtop, curr_char);
										if (U_ISPRINT(curr_char))
											break;
									}
									strcursor = strprev;		/* back up cursor */
									GTM_IO_WCWIDTH(curr_char, char_width);
									io_ptr->dollar.x -= char_width;
								}
							} else
#endif
								io_ptr->dollar.x--;
						str = strnext;
						utf8_crlast = FALSE;
						break;
					case NATIVE_FF:
						io_ptr->dollar.x = io_ptr->dollar.y = 0;
						str = strstart = strcursor = strnext;
						utf8_crlast = FALSE;
						break;
					case NATIVE_ESC:
						utf8_crlast = FALSE;
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
						utf8_crlast = FALSE;
						if (!gtm_utf8_mode)
						{
							if (!(pattern_typemask[*str] & PATM_C))
								io_ptr->dollar.x++;
							str++;
						}
						UNICODE_ONLY(
						else
						{
							assert(str < strtop);	/* PATTERN_TYPEMASK macro relies on this */
							if (utf8_term)
							{
								if (curr_char == u32_line_term[U32_LT_NL] ||
									curr_char == u32_line_term[U32_LT_LS] ||
									curr_char == u32_line_term[U32_LT_PS])
								{	/* a line terminator not handled above */
									io_ptr->dollar.y++;
									if (io_ptr->length)
										io_ptr->dollar.y %= io_ptr->length;
									io_ptr->dollar.x = 0;
									strstart = strcursor = strnext;
									char_width = 0;
								} else
									GTM_IO_WCWIDTH(curr_char, char_width);
								if (0 < char_width)
								{
									io_ptr->dollar.x += char_width;
									strcursor = strnext;
								}

							} else if (U_ISPRINT(curr_char))
								io_ptr->dollar.x++;
							str = strnext;
						}
						)	/* UNICODE_ONLY */
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
#ifdef UNICODE_SUPPORTED
				if (utf8_term)
				{
					GTM_IO_WCWIDTH(curr_char, char_width);
					io_ptr->dollar.x += char_width;
				} else
#endif
					io_ptr->dollar.x++;
				str = strnext;
			}
		}
#ifdef UNICODE_SUPPORTED
	} else if (utf8_active)
	{
		for (total = 0; str < strtop; str = strnext)
		{
			strnext = UTF8_MBTOWC(str, strtop, curr_char);
			if (utf8_term)
			{	/* count display width */
				GTM_IO_WCWIDTH(curr_char, char_width);
				total += char_width;
			} else
				total++;	/* count number of Unicode characters */
		}
		io_ptr->dollar.x += total;
#endif
	} else
		io_ptr->dollar.x += (unsigned int)(strtop - str);
	if (io_ptr->dollar.x > io_ptr->width && io_ptr->wrap)
	{
		io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}
}
