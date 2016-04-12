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
/* The search starts at the hint and does not wrap 		*/

int4 bm_find_blk(int4 hint, sm_uc_ptr_t base_addr, int4 total_bits, boolean_t *used)
{
	int4		bits;
	sm_uc_ptr_t 	ptr, top;
	unsigned char	valid;

	assert(hint < total_bits);
	total_bits *= BML_BITS_PER_BLK;
	/* Currently bm_find_blk has been coded to assume that if "hint" is
	 * non-zero then it is less than total_bits. We better assert that.
	 */
	if (hint)
	{
		ptr = base_addr + (hint * BML_BITS_PER_BLK) / 8;
		top = base_addr + (total_bits + 7) / 8 - 1;
		if (ptr == top)
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
		}
		else
			valid = *ptr;
		switch (hint % (8 / BML_BITS_PER_BLK))
		{
		        case 0:	break;
			case 1:	valid = valid & 252;
				break;
			case 2: valid = valid & 240;
				break;
			case 3: valid = valid & 192;
				break;
		}
		if (valid)
		{
			if (valid & 1)
			{
				if (valid & 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
			}
			else if (valid & 4)
			{
				if (valid & 8)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
			}
			else if (valid & 16)
			{
				if (valid & 32)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
			}
			else
			{
				if (valid & 128)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
			}
		}
		ptr++;
	}
	else
		ptr = base_addr;

	for (top = base_addr + (total_bits +7) / 8 - 1; ptr < top; ptr++)
	{
		if (*ptr)
		{
			if (*ptr & 1)
			{
				if (*ptr & 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
			}
			else if (*ptr & 4)
			{
				if (*ptr & 8)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
			}
			else if (*ptr & 16)
			{
				if (*ptr & 32)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
			}
			else
			{	if (*ptr & 128)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
			}
		}
	}
	if ((ptr == top) && *ptr)	/* Special processing for last byte--may be only partially valid , if had hint may */
	{
		bits = total_bits % 8;	/* have already done last byte, then ptr will be greater than top */
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
			if (valid & 1)
			{
				if (valid & 2)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
			}
			else if (valid & 4)
			{
				if (valid & 8)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
			}
			else if (valid & 16)
			{
				if (valid & 32)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
			}
			else
			{
				if (valid & 128)
					*used = TRUE;
				else
					*used = FALSE;
				return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
			}
		}
	}
	return -1;
}
