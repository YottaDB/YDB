/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"

#define MAX_FFS_SIZE 32

/* Returns the location of the first set bit in the field.	*/
/* The search starts at the hint and wraps if necessary.	*/

int4 bml_find_free(int4 hint, uchar_ptr_t base_addr, int4 total_bits, boolean_t *used)
{
	uchar_ptr_t	ptr, top;
	unsigned char 	valid;
	int4		bits;

	hint *= BML_BITS_PER_BLK;
	hint = hint / 8;
	total_bits *= BML_BITS_PER_BLK;

	if (hint > total_bits)
		hint = 0;
	for (ptr = base_addr + hint, top = base_addr + (total_bits +7) / 8 - 1; ptr < top; ptr++)
	{	if (*ptr)
		{
			if (*ptr > 63)
			{
				if (*ptr > 127)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3 );
			}
			else if (*ptr > 15)
			{
				if (*ptr > 31)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2 );
			}
			else if (*ptr > 3)
			{
				if (*ptr > 7)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1 );
			}
			else
			{
				if (*ptr == 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) );
			}
		}
	}
	if (*ptr)	/* Special processing for last byte as may be only partially valid */
	{
		bits = total_bits % 8;
		if (bits == 6)
			valid = *ptr & 63;
		else if (bits == 4)
			valid = *ptr & 15;
		else if (bits == 2)
			valid = *ptr & 3;
		else
			valid = *ptr;
		if (valid)
		{
			if (valid > 63)
			{
				if (valid > 127)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3 );
			}
			else if (valid > 15)
			{
				if (valid > 31)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2 );
			}
			else if (valid > 3)
			{
				if (valid > 7)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1 );
			}
			else
			{
				if (valid == 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) );
			}
		}
	}
	for (ptr = base_addr, top = base_addr + hint; ptr < top; ptr++)
	{
		if (*ptr)
		{
			if (*ptr > 63)
			{
				if (*ptr > 127)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3 );
			}
			else if (*ptr > 15)
			{
				if (*ptr > 31)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2 );
			}
			else if (*ptr > 3)
			{
				if (*ptr > 7)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1 );
			}else
			{
				if (*ptr == 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) );
			}
		}
	}
	return -1;
}
