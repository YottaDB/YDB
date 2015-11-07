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
#include "error.h"
#include <climsgdef.h>
#include <descrip.h>
#include "gtm_string.h"
#include "cli.h"

extern int4 CLI$GET_VALUE();
extern int CLI$PRESENT();
/* This routine is written as a parallel module of UNIX cli_get_str(),
	which always returns the concatenated string.
	cli_get_str() of VVMS returns one piece at a time.
   Note: cli_text must be a null terminated string below */
cli_get_str_all_piece(unsigned char *cli_text, unsigned char *all_piece_buff, int *all_piece_buff_len)
{
	unsigned char                   map_pool[256], *ptr;
	int				status, s_len;
	unsigned short			n_len;

	struct dsc$descriptor_s desc_cli_str;
	$DESCRIPTOR(buffer, map_pool);
        desc_cli_str.dsc$a_pointer = cli_text;
        desc_cli_str.dsc$w_length = strlen(cli_text);
        desc_cli_str.dsc$b_dtype = DSC$K_DTYPE_T;
        desc_cli_str.dsc$b_class = DSC$K_CLASS_S;
	*all_piece_buff_len = s_len = 0;
	for (; ;)
	{
		status = CLI$GET_VALUE(&desc_cli_str, &buffer, &n_len);
		if (SS$_NORMAL == status)
		{
			memcpy(all_piece_buff + s_len, buffer.dsc$a_pointer, n_len);
			s_len += n_len;
			all_piece_buff[s_len] = 0;
			break;
		}
		if (CLI$_ABSENT == status)
			return FALSE;
		assert(CLI$_COMMA == status);
		if (CLI$_COMMA == status)
		{
			memcpy(all_piece_buff + s_len, buffer.dsc$a_pointer, n_len);
			s_len += n_len;
			all_piece_buff[s_len]=',';
			s_len++;
		}
	}
	*all_piece_buff_len = s_len;
	return TRUE;
}
