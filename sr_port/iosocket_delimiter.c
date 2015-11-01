/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_delimiter.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_string.h"

#include "gtm_inet.h"

#include "io.h"
#include "iottdef.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	gtm_utf8_mode;
GBLREF  UConverter      *chset_desc[];

error_def(ERR_DELIMSIZNA);

boolean_t iosocket_delimiter(unsigned char *delimiter_buffer, int4 delimiter_len, socket_struct *socketptr, boolean_t rm)
{
	int		counter, ii, c_len;
	unsigned char	*c, *top, delimiter[MAX_DELIM_LEN + 1];

	/* free the previous delimiters if any */
	for (ii = 0; ii < socketptr->n_delimiter; ii++)
	{
		free(socketptr->delimiter[ii].addr);
		if (socketptr->idelimiter[ii].addr != socketptr->delimiter[ii].addr)
			free(socketptr->idelimiter[ii].addr);
	}
	if (0 < socketptr->n_delimiter && socketptr->odelimiter0.addr != socketptr->delimiter[0].addr)
		free(socketptr->odelimiter0.addr);
	socketptr->n_delimiter = 0;
	socketptr->delim0containsLF = FALSE;
	if (rm)
		return TRUE;
	/* fill in the new delimiters */
	counter = ii = 0;
	c = &delimiter_buffer[0];
	top = c + delimiter_len;
	while ((c < top) && (ii < MAX_N_DELIMITER))
	{
		switch(delimiter[counter++] = *c++)
		{
			case ':' :
				/* end of the previous delimiter and start the next one */
				if (1 < counter)
				{
					if (gtm_utf8_mode)
					{	/* Check if delimiter has any invalid UTF-8 characters */
						c_len = utf8_len_strict(delimiter, counter - 1);
					} else
						c_len = counter - 1;
					socketptr->delimiter[ii].addr = (char *)malloc(counter - 1);
					memcpy(socketptr->delimiter[ii].addr, delimiter, counter - 1);
					socketptr->delimiter[ii].len = counter - 1;
					UNICODE_ONLY(socketptr->delimiter[ii].char_len = c_len);
					socketptr->idelimiter[ii] = socketptr->delimiter[ii];
					if (0 == ii)
						socketptr->odelimiter0 = socketptr->delimiter[0];
					socketptr->n_delimiter++;
					ii++;
				}
				counter = 0;
				break;
			case '/' : /* escape */
				delimiter[counter - 1] = *c++; /* Escaping a delim character doesn't appear to be documented
								  anywhere. Nonetheless, the assumption is that the only use is to
								  escape ':' which is a one byte character in UTF-8. So, this logic
								  will work. However, if there is a change in what can be escaped
								  (say, escape a character that is > 1 byte in length), this logic
								  has to change. Vinaya, 2007/09/07
							       */
				break;
			case NATIVE_LF : /* Only NATIVE_LF is accepted as line terminator although Unicode defines other
					    line terminators */
				if (0 == ii)
					socketptr->delim0containsLF = TRUE;
				break;
			default:
				/* look at the next character */
				break;
		}
		if ((c == top) && (0 < counter))
		{
			if (gtm_utf8_mode) /* Check if delimiter has any invalid UTF-8 character */
				c_len = utf8_len_strict(delimiter, counter); /* triggers badchar error for invalid sequence */
			else
				c_len = counter;
			socketptr->delimiter[ii].addr = (char *)malloc(counter);
			memcpy(socketptr->delimiter[ii].addr, delimiter, counter);
			socketptr->delimiter[ii].len = counter;
			UNICODE_ONLY(socketptr->delimiter[ii].char_len = c_len);
			socketptr->idelimiter[ii] = socketptr->delimiter[ii];
			if (0 == ii)
				socketptr->odelimiter0 = socketptr->delimiter[0];
			socketptr->n_delimiter++;
			ii++;
		}
		if (counter > MAX_DELIM_LEN)
		{
			rts_error(VARLSTCNT(1) ERR_DELIMSIZNA);
			return FALSE;
		}
	}
	return TRUE;
}

void iosocket_delim_conv(socket_struct *socketptr, gtm_chset_t to_chset)
{
	static char	*conv_buff = NULL;
	int		conv_len, delim_index, new_delim_len;

	assert(0 != socketptr->n_delimiter);
	assert(CHSET_UTF16BE == to_chset || CHSET_UTF16LE == to_chset);
	assert(gtm_utf8_mode);

	if (NULL == conv_buff)
		conv_buff = malloc(MAX_DELIM_LEN);
	for (delim_index = 0; delim_index < socketptr->n_delimiter; delim_index++)
	{
		conv_len = MAX_DELIM_LEN;
		new_delim_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[to_chset], &socketptr->delimiter[delim_index],
					 conv_buff, &conv_len);
		assert(MAX_DELIM_LEN > new_delim_len);
		if (MAX_DELIM_LEN < new_delim_len)
		{
			rts_error(VARLSTCNT(1) ERR_DELIMSIZNA);
			return;
		}
		socketptr->idelimiter[delim_index].len = new_delim_len;
		UNICODE_ONLY(socketptr->idelimiter[delim_index].char_len = socketptr->delimiter[delim_index].char_len);
		socketptr->idelimiter[delim_index].addr = malloc(new_delim_len);
		memcpy(socketptr->idelimiter[delim_index].addr, conv_buff, new_delim_len);
	}
	return;
}

void iosocket_delimiter_copy(socket_struct *from, socket_struct *to)
{
	int	delim_index;

	if (0 == (to->n_delimiter = from->n_delimiter))
		return;
	for (delim_index = 0; delim_index < from->n_delimiter; delim_index++)
	{
		to->delimiter[delim_index] = from->delimiter[delim_index]; /* copy all fields */
		to->delimiter[delim_index].addr = malloc(from->delimiter[delim_index].len); /* re-allocate buffer */
		memcpy(to->delimiter[delim_index].addr, from->delimiter[delim_index].addr, from->delimiter[delim_index].len);
		to->idelimiter[delim_index] = to->delimiter[delim_index]; /* copy all fields */
		if (from->delimiter[delim_index].addr != from->idelimiter[delim_index].addr)
		{ /* has been converted */
			to->idelimiter[delim_index].addr = malloc(from->idelimiter[delim_index].len); /* re-allocate buffer */
			memcpy(to->idelimiter[delim_index].addr, from->idelimiter[delim_index].addr,
					from->idelimiter[delim_index].len);
		}
	}
	to->odelimiter0 = to->delimiter[0];
	if (from->odelimiter0.addr != from->delimiter[0].addr)
	{ /* has been converted */
		to->odelimiter0.addr = malloc(from->odelimiter0.len);
		memcpy(to->odelimiter0.addr, from->odelimiter0.addr, from->odelimiter0.len);
	}
	return;
}
