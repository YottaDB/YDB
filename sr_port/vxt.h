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

#define VXT_END (-1)	/* end of a triple template */
#define VXT_VAL (-2)	/* triple operand number val.n follows in next slot */
#define VXT_ADDR (-3)	/* like above but addr.n */
#define VXT_XFER (-4)	/* transfer vector offset follows in next slot */
#define VXT_LIT (-5)	/* integer literal follows in next slot */
#define VXT_IREPL (-6)	/* this is an irep instruction (pseudo-push) */
#define VXT_IREPAB (-7)	/* this is an irep instruction (pseudo-push) */
#define VXT_REG (-8)	/* a VAX operand specifier follows in the next slot */
#define VXT_JMP (-9)
#define VXT_GREF (-10)
