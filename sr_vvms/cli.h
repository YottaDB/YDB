/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLI$_NEGATED
#include <climsgdef.h>
#endif

#define CLI_NEGATED CLI$_NEGATED
#define CLI_PRESENT CLI$_PRESENT
#define CLI_ABSENT  CLI$_ABSENT
#define MAX_LINE 512
#define CLI_GET_STR_ALL	cli_get_str_all_piece

/* include platform independent prototypes */

#include "cliif.h"

int cli_get_str_all_piece(unsigned char *cli_text, unsigned char *all_piece_buff,
	int *all_piece_buff_len); /***type int added***/
