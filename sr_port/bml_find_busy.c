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
#include "bml_find_busy.h"

#define MAX_FFS_SIZE 32

/* Returns the location of the first clear bit in the field.	*/
/* The search starts at the hint and wraps if necessary.	*/

int4 bml_find_busy(int4 hint, uchar_ptr_t base_addr, int4 total_blks)
{
	uchar_ptr_t	ptr, top;
	unsigned char 	valid;
	int4		bits, hint_pos, total_bits, ret_val;

	hint_pos = hint * BML_BITS_PER_BLK;
	total_bits = total_blks * BML_BITS_PER_BLK;
	if (hint_pos >= total_bits)
		hint_pos = 0;
	hint_pos = hint_pos / 8;

	for (ptr = base_addr + hint_pos, top = base_addr + (total_bits + 7) / 8 - 1; ptr < top; ptr++)
	{
	  	if ((*ptr & 0x3) == 0)
		{
			ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
			if (ret_val >= hint)
				return ret_val;
		}
		if ((*ptr & 0xc) == 0)
		{
			ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
			if (ret_val >= hint)
				return ret_val;
		}
		if ((*ptr & 0x30) == 0)
		{
			ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
			if (ret_val >= hint)
				return ret_val;
		}
		if ((*ptr & 0xc0) == 0)
		{
			ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
			if (ret_val >= hint)
				return ret_val;
		}
	}

	bits = total_bits % 8;
	if (bits == 6)
		valid = *ptr | 0xc0;
	else if (bits == 4)
		valid = *ptr | 0xf0;
	else if (bits == 2)
		valid = *ptr | 0xfc;
	else
		valid = *ptr;

	if ((valid & 0x3) == 0)
	{
		ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
		if (ret_val >= hint)
			return ret_val;
	}
	if ((valid & 0xc) == 0)
	{
		ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
		if (ret_val >= hint)
			return ret_val;
	}
	if ((valid & 0x30) == 0)
	{
		ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
		if (ret_val >= hint)
			return ret_val;
	}
	if ((valid & 0xc0) == 0)
	{
		ret_val = (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
		if (ret_val >= hint && ret_val <= total_blks)
			return ret_val;
	}

	for (ptr = base_addr, top = base_addr + hint_pos; ptr < top; ptr++)
	{
		if ((*ptr & 0x3) == 0)
		{
			return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
		}
		else if ((*ptr & 0xc) == 0)
		{
			return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
		}
		else if ((*ptr & 0x30) == 0)
		{
			return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
		}
		else if ((*ptr & 0xc0) == 0)
		{
			return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);
		}
	}

	bits = hint % 4;
	if      (bits == 3)
		valid = *ptr | 0xc0;
	else if (bits == 2)
		valid = *ptr | 0xf0;
	else if (bits == 1)
		valid = *ptr | 0xfc;
	else
		valid = *ptr;

	if ((valid & 0x3) == 0)
		return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK));
	if ((valid & 0xc) == 0)
		return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 1);
	if ((valid & 0x30) == 0)
		return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 2);
	if ((valid & 0xc0) == 0)
		return (int4)((ptr - base_addr) * (8 / BML_BITS_PER_BLK) + 3);

	return -1;
}
