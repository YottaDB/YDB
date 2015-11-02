/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DO_XFORM_INCLUDED
#define DO_XFORM_INCLUDED

#define	DO_XFORM_RETURN_IF_NULL_STRING(INPUT, OUTPUT, LENGTH)								\
{															\
	if (!INPUT->len)												\
	{	/* If input string is the null subscript, we want the output (of the collation transformation function)	\
		 * to be a null subscript as well. This is because the null subscript has special meaning as far as	\
		 * GT.M subscript collation is concerned. In case the collation routine decides to map it to a non-null	\
		 * subscript the user might start seeing undesirable collation orders. To be safe and avoid such	\
		 * issues, the null subscript is handled specially by mapping it to a null subscript internally by GT.M	\
		 * (without passing this to the collation routine) and returning right away.				\
		 */													\
		OUTPUT->len = 0;											\
		*LENGTH = 0;												\
	}														\
}

void do_xform(collseq *csp, int fc_type, mstr *input, mstr *output, int *length);
/*
 * fc_type would be either XFORM (0) or XBACK (1)
 */
#endif /* DO_XFORM_INCLUDED */
