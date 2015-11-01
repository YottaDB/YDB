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

/* iosocket_delimiter.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_string.h"

#include <netinet/in.h>

#include "io.h"
#include "iottdef.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"

boolean_t iosocket_delimiter(unsigned char *delimiter_buffer, int4 delimiter_len, socket_struct *socketptr, boolean_t rm)
{
	int		counter, ii;
	unsigned char	*c, *top, delimiter[MAX_DELIM_LEN + 1];

	error_def(ERR_DELIMSIZNA);

	/* free the previous delimiters if any */
	for (ii = 0; ii < socketptr->n_delimiter; ii++)
		free(socketptr->delimiter[ii].addr);
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
		case    ':' :
			/* end of the previous delimiter and start the next one */
			if (1 < counter)
			{
				socketptr->delimiter[ii].addr = (char *)malloc(counter - 1);
				memcpy(socketptr->delimiter[ii].addr, delimiter, counter - 1);
				socketptr->delimiter[ii++].len = counter - 1;
				socketptr->n_delimiter++;
			}
			counter = 0;
			break;
		case    '/' :
			/* escape */
			delimiter[counter - 1] = *c++;
			break;
		case NATIVE_LF :
			if (0 == ii)
				socketptr->delim0containsLF = TRUE;
			break;
		default:
			/* look at the next character */
			break;
		}
		if ((c == top) && (0 < counter))
		{
			socketptr->delimiter[ii].addr = (char *)malloc(counter);
			memcpy(socketptr->delimiter[ii].addr, delimiter, counter);
			socketptr->delimiter[ii++].len = counter;
			socketptr->n_delimiter++;
		}
		if (counter > MAX_DELIM_LEN)
		{
			rts_error(VARLSTCNT(1) ERR_DELIMSIZNA);
			return FALSE;
		}
	}
	return TRUE;
}
