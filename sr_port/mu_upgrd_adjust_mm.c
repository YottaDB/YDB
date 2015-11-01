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
#include "mu_upgrd.h"

#define BITS_PER_UCHAR 8


/*-------------------------------------------------------------------------
  Adjust master map according to the way data is organized after mupip upgrade.
  This assumes that we have at least 3 local bit maps (i.e. 3 blocks)
  A graphic example is better to explain:
  Each letter corresponds to a bit
	old permutation:  H G F E D C B A   P O N M L K J I   U T S R Q
	new permutation:  I H G F E D C A   Q P O N M L K J   U B T S R
  Note: for non-existance local map, master bit contains a '1'
  -------------------------------------------------------------------------*/
void mu_upgrd_adjust_mm(unsigned char *m_map, int4 tot_bits)
{
	unsigned char  *ptr, second_bit, mask, last_bit, last_carry, carry;
	int		last_byte_bits, tot_bytes, i;

	tot_bytes = DIVIDE_ROUND_UP(tot_bits, BITS_PER_UCHAR);
	last_byte_bits =  tot_bits - (tot_bytes - 1) *  BITS_PER_UCHAR;

	second_bit = ((*m_map) & 0x02) >> 1;

	/* start from last byte */
	ptr = m_map + tot_bytes -1;

	if (last_byte_bits == 1)
		carry = second_bit;

	else 	/* last_byte_bits >=2 */
	{
		carry = (*ptr) & 0x01;
		mask =  0x01 << (last_byte_bits - 1);
		last_bit = (*ptr) & mask; 	/* save last bit. Will copy back to same position */
		*ptr = ((*ptr) >> 1) | 0x80;    /* right shift and fill gap with one */
		mask = mask | (mask >> 1); 	/* will change last two bits at a time */
		*ptr = *ptr & (~mask);          /* make zeros in the two bits */
		*ptr = *ptr | last_bit | (second_bit << (last_byte_bits - 2));
	}


	for (i=0; i< tot_bytes - 1; i++)
	{
		ptr--;
		last_carry = carry; 			/* save last carry */
		carry = (*ptr) & 0x01;			/* save current byte LSB */
		*ptr = (*ptr >> 1) | (last_carry << 7); /* right shift and fill with last carry */
	}

	*ptr = (*ptr & 0xfe) | carry;  /* first bit saved as carry will be copied back */
}

