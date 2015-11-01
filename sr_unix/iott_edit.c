/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
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

#include "io.h"
#include "trmdef.h"
#include "iottdef.h"
#include "iottdefsp.h"
#include "iott_edit.h"
#include "gtmio.h"
#include "eintr_wrappers.h"

GBLREF io_pair 		io_curr_device;

GBLREF int		AUTO_RIGHT_MARGIN, EAT_NEWLINE_GLITCH;
GBLREF char		*CURSOR_UP, *CURSOR_DOWN, *CURSOR_LEFT, *CURSOR_RIGHT;

int 	write_str(unsigned char *str, unsigned short len, unsigned short cur_x, bool move)
{
	/*
	  writes a specified string starting from the current cursor position
	  and returns the cursor back to the same place.

		str    -> the string to write
		len    -> the length of the string
		cur_x  -> is the current cursor's column in the window.
		move   -> whether the cursor moves or not.

	  returns -1 if error, 0 if successful
	*/

	int	k, number_of_lines_up, number_of_chars_left, written, ret;
	int	width = io_curr_device.in->width;
	int	fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp)) -> fildes;
	io_desc *io_ptr = io_curr_device.in;

	assert(width);
	number_of_lines_up = (cur_x + len) / width;
	number_of_chars_left = (cur_x + len) % width - (cur_x) % width;

    	for (k = 0;  k < number_of_lines_up;  k++)
    	{
		int	cur_width;

		if (k == 0)
			cur_width = width - cur_x;
		else
			cur_width = width;
		if (cur_width)
		{
	    		DOWRITERL_A(fildes, str, cur_width, written);
			if (0 > written)
				return -1;
		}
	    	str += written;
	    	len -= written;

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
    	}
	if (len)
	{
		DOWRITERL_A(fildes, str, len, written);
		if (0 > written)
			return -1;
	}
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
		}
		else
		{
			ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, -number_of_chars_left);
			if (0 > ret)
				return -1;
		}
	}
	return 0;
}


int 	move_cursor_left(unsigned short col)
{
	/* -------------------------------------------------------
	 *  moves cursor left by one column.  if col is leftmost,
	 *  then it goes back to the end of the previous line
	 *  returns 0 if success, != 0 if error
	 * -------------------------------------------------------
	 */

	int	fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp)) -> fildes;
	int	ret;


	if (col > 0)
	{
		DOWRITERC(fildes, CURSOR_LEFT, strlen(CURSOR_LEFT), ret);
	}
	else
	{
		DOWRITERC(fildes, CURSOR_UP, strlen(CURSOR_UP), ret);
		if (0 > ret)
			return -1;
		ret = write_loop(fildes, (unsigned char *)CURSOR_RIGHT, io_curr_device.in->width - 1);
	}
	return ret;
}


int 	move_cursor_right(unsigned short col)
{
	/*
		moves cursor right by one column. if col is rightmost,
		then it goes to the start of the next line
		returns 0 if success, != 0 if error
	*/

	int	fildes = ((d_tt_struct *)((io_curr_device.in)->dev_sp)) -> fildes;
	int	ret;
	io_desc *io_ptr = io_curr_device.in;


	if (col < io_curr_device.in->width - 1)
	{
		DOWRITERC(fildes, CURSOR_RIGHT, strlen(CURSOR_RIGHT), ret);
	}
	else
	{
		DOWRITERC(fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL), ret);
	}
	return ret;
}


int	write_loop(int fildes, unsigned char *str, int num_times)
{
	int		i, size_required, ret;
	unsigned char	string[1024];
	unsigned char	*temp = NULL;

	*string = '\0';
	size_required = num_times * strlen((char *)str);

	if (size_required > sizeof(string))
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
	}
	else if (num_up > 0)
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
	}
	else if (num_left > 0)
	{
		ret = write_loop(fildes, (unsigned char *)CURSOR_LEFT, num_left);
		if (0 > ret)
			return -1;
	}
	return 0;
}
