/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* WARNING: this module contains a mixture of ASCII and EBCDIC on S390*/
#include "mdef.h"

#include "gtm_string.h"
#include <signal.h>
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include <wctype.h>

#include "io.h"
#include "trmdef.h"
#include "iottdef.h"
#include "iottdefsp.h"
#include "iott_edit.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif
#include "min_max.h"

GBLREF io_pair 		io_curr_device;

GBLREF int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLREF char		*CURSOR_UP, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT;

int	iott_write_raw(int fildes, void *str832, unsigned int len)
{
	/* Writes len characters without considering the cursor position
	 * The characters are really Unicode codepoints if the current
	 * device is in UTF-8 mode
	 * Returns -1 if error or number of bytes written
	 */
	unsigned char	*str, string[TTDEF_BUF_SZ], *outptr, *outtop;
	wint_t		*str32, temp32;
	int		written, this_write, outlen;
	io_desc		*io_ptr = io_curr_device.in;
	d_tt_struct	*tt_ptr;
	boolean_t	utf8_active;

	if (0 == len)
		return 0;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	if (!utf8_active)
	{
		str = (unsigned char *)str832;
		DOWRITERL_A(fildes, str, len, written);
		if (0 > written)
			return -1;
#ifdef UNICODE_SUPPORTED
	} else
	{
		str32 = (wint_t *)str832;
		for (written = 0; 0 < len; )
		{
			for (outptr = string, outtop = (unsigned char *)(string + SIZEOF(string)); 0 < len &&
				(outptr + GTM_MB_LEN_MAX) < outtop; str32++, len--)
			{
				temp32 = *str32;	/* so argument not modified */
				outptr = UTF8_WCTOMB(temp32, outptr);
			}
			/* either end of input or end of conversion buffer */
			if (0 < (outlen = (int)((outptr - string))))
			{	/* something to write */
				DOWRITERL_A(fildes, string, outlen, this_write);
				if (0 > this_write)
					return -1;
				written += this_write;
			}
		}
#endif
	}
	return written;
}

int 	write_str(void *str832, unsigned int len, unsigned int start_x, boolean_t move, boolean_t multibyte)
{
	/*
	  writes a specified string starting from the current cursor position
	  and returns the cursor back to the same place.

		str832		-> the string to write, may be Unicode code points
		len		-> the length of the string
		start_x		-> is the current cursor's column in the window.
		move		-> whether the cursor moves or not.
		multibyte	-> str832 is always bytes, if utf8_active then multibyte possible

	  returns -1 if error, 0 if successful
	*/

	int		number_of_lines_up, number_of_chars_left, written, ret, outlen;
	unsigned int	cur_x;
	int		width = io_curr_device.in->width, cur_width, line_width, this_width;
	int		fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp))->fildes;
	unsigned char	*str, string[TTDEF_BUF_SZ], *outptr, *outtop, *strstart, *nextptr;
	wint_t		*str32, temp32;
	io_desc		*io_ptr = io_curr_device.in;
	d_tt_struct	*tt_ptr;
	boolean_t	utf8_active, writenewline;

	assert(width);
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	if (utf8_active && !multibyte)
		str32 = (wint_t *)str832;
	else
		str = (unsigned char *)str832;
	if (multibyte && utf8_active)
	{
		outptr = str;
		outtop = str + len;
	}
	cur_x = start_x % width;	/* start_x is absolute, get relative value into cur_x */
	number_of_lines_up = 0;
#ifdef UNICODE_SUPPORTED
	if (utf8_active)
	{
		cur_width = width - cur_x;
		for (line_width = 0; 0 < len; )
		{
			writenewline = FALSE;
			if (multibyte)
			{	/* go through mb string one line at a time */
				for (strstart = outptr ; outptr < outtop; )
				{
					nextptr = UTF8_MBTOWC(outptr, outtop, temp32);
					GTM_IO_WCWIDTH(temp32, this_width);
					if ((line_width + this_width) > cur_width)
					{
						writenewline = TRUE;
						break;
					}
					line_width += this_width;
					outptr = nextptr;
				}
				outlen = (int)(outptr - strstart);
				len -= outlen;
			} else
			{
				for (outptr = string, outtop = string + SIZEOF(string);
					(0 < len) && ((outptr + GTM_MB_LEN_MAX) < outtop);
						str32++, len--)
				{
					GTM_IO_WCWIDTH(*str32, this_width);
					if ((line_width + this_width) > cur_width)
					{
						writenewline = TRUE;
						break;
					}
					line_width += this_width;
					temp32 = *str32;	/* preserve argument */
					outptr = UTF8_WCTOMB(temp32, outptr);
				}
				outlen =(int)(outptr - string);
				strstart = string;
			}
			assert(!writenewline || len);
			assert(line_width <= cur_width);
			if (line_width >= cur_width)	/* our write has positioned us to end of width. write new line */
				writenewline = TRUE;
			/* either end of input, end of conversion buffer, or full line */
			if (0 < outlen)
			{	/* something to write */
				DOWRITERL_A(fildes, strstart, outlen, written);
				if (0 > written)
					return -1;
			}
			if (!writenewline)
			{
				if (0 < len)
					continue;	/* end of buffer in middle of line so no EOL */
				if (0 == len)
				{	/* last line is partial so no new line */
					cur_x = cur_x + line_width;
					break;
				}
			}
			/* -------------------------------------------------------------------------
			* for terminals that have the EAT_NEWLINE_GLITCH auto_margin doesn't work
			* even though AUTO_RIGHT_MARGIN may be 1. So you have to check both
			* before writing the TTEOL
			* -------------------------------------------------------------------------
			*/
			if (!AUTO_RIGHT_MARGIN || EAT_NEWLINE_GLITCH)
			{
				DOWRITERC(fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL), ret);
				if (0 != ret)
					return -1;
			}
			number_of_lines_up++;
			cur_x = line_width = 0;
			cur_width = width;
		}
	} else
	{
#endif
		for ( ; 0 < len; )
		{
			cur_width = width - cur_x;
			if (cur_width)
			{
				if (len < cur_width)
					cur_width = len;
				DOWRITERL_A(fildes, str, cur_width, written);
				if (0 > written)
					return -1;
				str += written;
				len -= written;
				cur_x += cur_width;
			}
			if ((cur_x < width) && (0 == len))
				break;	/* last line is partial so no new line */
			/* -------------------------------------------------------------------------
		 	 * for terminals that have the EAT_NEWLINE_GLITCH auto_margin doesn't work
		 	 * even though AUTO_RIGHT_MARGIN may be 1. So you have to check both
		 	 * before writing the TTEOL
		 	 * -------------------------------------------------------------------------
		 	 */
			if (!AUTO_RIGHT_MARGIN || EAT_NEWLINE_GLITCH)
			{
		    		DOWRITERC(fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL), ret);
				if (0 != ret)
					return -1;
			}
			number_of_lines_up++;
			cur_x = 0;
			cur_width = width;
		}
#ifdef UNICODE_SUPPORTED
	}
#endif
	number_of_chars_left = cur_x - start_x;
	if (!move)
	{
		ret = write_loop(fildes, (unsigned char *)CURSOR_UP, number_of_lines_up);
		if (0 > ret)
			return -1;
		if (number_of_chars_left > 0)
		{
			ret = write_loop(fildes, (unsigned char *)CURSOR_LEFT, number_of_chars_left);
			if (0 > ret)
				return -1;
		} else
		{
			ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, -number_of_chars_left);
			if (0 > ret)
				return -1;
		}
	}
	return 0;
}

/* This function is almost the same as "write_str" except that the string that it writes is spaces. It uses "write_str" in turn */
int 	write_str_spaces(unsigned int len, unsigned int start_x, boolean_t move)
{
	static unsigned char	*space_buf = NULL;
	static int		buflen = 0;
	int			i;

	if (len > buflen)
	{
		if (NULL != space_buf)
			free(space_buf);
		space_buf = (unsigned char *)malloc(len);
		buflen = len;
		for (i = 0; i < buflen; i++)
			space_buf[i] = ' ';
	}
	/* Currently "write_str" treats any input string as a "wint_t" array if in utf8 mode. To avoid that we take the
	 * sleazy route of calling it with "multibyte" option (last parameter) set to TRUE that way it treats this as
	 * a byte array rather than a wint_t array.
	 */
	return write_str(&space_buf[0], len, start_x, move, TRUE);
}

int 	move_cursor_left(int col, int num_cols)
{
	/* -------------------------------------------------------
	 *  moves cursor left by num_cols columns.  if col is leftmost,
	 *  then it goes back to the end of the previous line
	 *  returns 0 if success, != 0 if error
	 * -------------------------------------------------------
	 */
	int	fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp))->fildes;
	int	ret;

	if (0 == num_cols)
		ret = 0;
	else if (0 > num_cols)
		ret = move_cursor_right(col, -num_cols);
	else if (0 < col)
	{
		ret = write_loop(fildes, (unsigned char *)CURSOR_LEFT, MIN(col, num_cols));
		num_cols -= MIN(col, num_cols);
		if (num_cols)
		{
			DOWRITERC(fildes, CURSOR_UP, strlen(CURSOR_UP), ret);
			if (0 > ret)
				return -1;
			ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, io_curr_device.in->width - num_cols);
		}
	} else
	{
		DOWRITERC(fildes, CURSOR_UP, strlen(CURSOR_UP), ret);
		if (0 > ret)
			return -1;
		ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, io_curr_device.in->width - num_cols);
	}
	return ret;
}

int 	move_cursor_right(int col, int num_cols)
{
	/* -------------------------------------------------------
	 *	moves cursor right by num_cols columns, if col is rightmost,
	 *	then it goes to the start of the next line
	 *	returns 0 if success, != 0 if error
	 * -------------------------------------------------------
	 */
	int	fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp))->fildes;
	int	ret;
	io_desc *io_ptr = io_curr_device.in;

	if (0 == num_cols)
		ret = 0;
	else if (0 > num_cols)
		ret = move_cursor_left(col, -num_cols);
	else if ((io_curr_device.in->width - num_cols) > col)
		ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, num_cols);
	else
	{
		DOWRITERC(fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL), ret);
		if (0 > ret)
			return -1;
		num_cols -= (io_curr_device.in->width - col);
		if (num_cols)
			ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, num_cols);
	}
	return ret;
}

int	write_loop(int fildes, unsigned char *str, int num_times)
{
	int		i, size_required, ret;
	unsigned char	string[1024];
	unsigned char	*temp = NULL;

	*string = '\0';
	size_required = num_times * STRLEN((char *)str);

	if (size_required > SIZEOF(string))
	{
		for (i = 0;  i < num_times;  i++)
		{
			DOWRITERC(fildes, str, strlen((char *)str), ret);
			if (0 > ret)
				return -1;
		}
	} else if (num_times)
	{
		for (i = 0;  i < num_times;  i++)
		{
			strcat((char *)string, (char *)str);
		}
		DOWRITERC(fildes, string, size_required, ret);
		if (0 > ret)
			return -1;
	}
	return 0;
}

int	move_cursor(int fildes, int num_up, int num_left)
{
	int	ret;

	if (num_up < 0)
	{
		ret = write_loop (fildes, (unsigned char *)CURSOR_DOWN, -num_up);
		if (0 > ret)
			return -1;
	} else if (num_up > 0)
	{
		ret = write_loop (fildes, (unsigned char *)CURSOR_UP, num_up);
		if (0 > ret)
			return -1;
	}

	if (num_left < 0)
	{
		ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, -num_left);
		if (0 > ret)
			return -1;
	} else if (num_left > 0)
	{
		ret = write_loop(fildes, (unsigned char *)CURSOR_LEFT, num_left);
		if (0 > ret)
			return -1;
	}
	return 0;
}

/* This function computes the absolute value of $X (i.e. without doing modulo the terminal device width)
 * for an arbitrary index in an array of characters. This requires the width of the terminal as input as
 * well as the starting display column (dx_start) for the 0th element of the character array. The final
 * value that it returns does not include "dx_start" as this is how "dx_instr" and "dx_outlen" is maintained
 * in the caller functions (dm_read and iott_readfl).
 *
 *	str832   -> the array of characters, may be Unicode code points
 *	index    -> the index of this array upto which we want to cumulative dollarx computed
 *	width    -> the WIDTH of the terminal device
 *	dx_start -> the value of $X after the GT.M prompt has been displayed (in case of dm_read) and 0 (for iott_readfl).
 *
 */
int 	compute_dx(void *str832, unsigned int index, unsigned int width, unsigned int dx_start)
{
	boolean_t	utf8_active;
	io_desc		*io_ptr = io_curr_device.in;
	wint_t		*str32;
	int		dx_ret, this_width, i;

	utf8_active = gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE;
	str32 = (wint_t *)str832;
	if (utf8_active)
	{
		dx_ret = dx_start;
		for (i = 0; i < index; i++)
		{
			GTM_IO_WCWIDTH(str32[i], this_width);
			assert(this_width <= width);	/* width cannot be less than one wide character's display width */
			if (((dx_ret % width) + this_width) > width)
				dx_ret = ROUND_UP(dx_ret, width);	/* add $X padding for wide character in last column */
			dx_ret += this_width;
		}
		return dx_ret - dx_start;	/* before returning make sure "dx_ret" is of same dimension as "dx_instr"
						 * variable in "dm_read" or "iott_readfl" (the callers) */
	} else
		return index;	/* every character is 1-display-column wide so no need to look at "str832" */
}
