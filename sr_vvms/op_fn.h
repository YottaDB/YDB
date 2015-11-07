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

/*	File modified by Maccrone on  4-DEC-1986 14:45:13.23    */
/*	File modified by Maccrone on  4-DEC-1986 08:28:30.71    */
typedef struct
{	char		len;
	char		name[20];
	short int 	item_code;
}dvi_struct;

typedef struct
{	char	index;
	char	len;
}dvi_index_struct;

typedef struct
{	short int	bufflen;
	short int	itmcode;
	int4		buffaddr;
	int4		retlen;
	int4		end;
}itmlist_struct;
