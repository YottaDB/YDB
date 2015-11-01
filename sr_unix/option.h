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

typedef struct
{	bool	present;
	union
	{	char	*str;
		int	num;
	}val;
}opt_com_arg;

typedef struct opt_com_struct	/* 	VMS equivalent qualifiers (: = takes argument)	*/
{		   /* 		MUPIP			|  		DSE			*/
opt_com_arg	a; /* : after				|					*/
opt_com_arg	A; /* : Adjacency			|					*/
opt_com_arg	b; /* : block block_density begin before| : block backward			*/
opt_com_arg	B; /*   Backward			|					*/
opt_com_arg	c; /*   comprehensive			| : count comment cmpc			*/
opt_com_arg	C; /*   Full				| : Current_tn				*/
opt_com_arg	d; /*					|   decimal				*/
opt_com_arg	D; /*					| : Data				*/
opt_com_arg	e; /* : error_limit end 		|   exhaustive				*/
opt_com_arg	E; /* : Extract				|					*/
opt_com_arg	f; /* : file				|   freeze fileheader freeblock free	*/
opt_com_arg	F; /*   Fast Freeze Forward		| : File From Forward Freeze		*/
opt_com_arg	g; /* : global				|   glo					*/
opt_com_arg	G; /* : Global_buffers			|					*/
opt_com_arg	h; /*					|   header hexadecimal			*/
opt_com_arg	H; /*					| : Hint				*/
opt_com_arg	i; /*   incremental interactive		|   init				*/
opt_com_arg	I; /* : ID				|					*/
opt_com_arg	j; /* : journal				|					*/
opt_com_arg	J; /*					|					*/
opt_com_arg	k; /*   keyranges			| : key key_max_size			*/
opt_com_arg	K; /*					|					*/
opt_com_arg	l; /* : log				| : level lower				*/
opt_com_arg	L; /* : Lookback limit (0 to negate)	|   List				*/
opt_com_arg	m; /* : map	(0 to negate)		|   master				*/
opt_com_arg	M; /* : Maxkeysize			|					*/
opt_com_arg	n; /* : name				|   nofreeze noheader			*/
opt_com_arg	N; /*   Nochecktn			| : Null_subscripts Number		*/
opt_com_arg	o; /*					| : offset				*/
opt_com_arg	O; /*					|   Owner				*/
opt_com_arg	p; /* : process				| : pointer				*/
opt_com_arg	P; /*					|   Path				*/
opt_com_arg	q; /*					|					*/
opt_com_arg	Q; /*					|					*/
opt_com_arg	r; /* : region				| : record region			*/
opt_com_arg	R; /*   Record Recover			|					*/
opt_com_arg	s; /* : show				|   star seize siblings			*/
opt_com_arg	S; /* : Subscript Since Select		|					*/
opt_com_arg	t; /* : transaction			| : tn to				*/
opt_com_arg	T; /*   Tn_reset			| : Total_blocks			*/
opt_com_arg	u; /* : user				| : upper				*/
opt_com_arg	U; /*					|					*/
opt_com_arg	v; /*   verify				| : version				*/
opt_com_arg	V; /*					|					*/
opt_com_arg	w; /*					|					*/
opt_com_arg	W; /*					|					*/
opt_com_arg	x; /*					|					*/
opt_com_arg	X; /*					|					*/
opt_com_arg	y; /*					|					*/
opt_com_arg	Y; /*					|					*/
opt_com_arg	z; /*					|					*/
opt_com_arg	Z; /*					|					*/
}opt_com;
