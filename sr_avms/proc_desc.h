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

/* proc_desc - Alpha OpenVMS procedure descriptor information.  */

typedef	struct
{
	short	flags;
	short	rsa_offset;
	short	reserved_1;
	short	signature_offset;
	int4	*code_address;
	int4	must_be_zero_1;		/* actually, rest of code_address, but DEC C only implements 32-bit pointers */
	int4	size;
} proc_desc;

#define PDSC_FLAGS	((PDSC$K_KIND_FP_STACK << PDSC$V_KIND) \
				| PDSC$M_BASE_REG_IS_FP | PDSC$M_NATIVE | PDSC$M_NO_JACKET | PDSC$M_HANDLER_VALID)
