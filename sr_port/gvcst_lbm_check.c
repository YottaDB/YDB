/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbml.h"
#include "gvcst_lbm_check.h"

/* Routines for checking status of database blocks using a given local bitmap */



/* There are two bits that denote the status of a block in the local bitmap:

   0b00 busy
   0b01 free
   0b10 invalid
   0b11 recycled (but free)

   The low order bit can be used to denote busy or free so that is what
   is actually tested.

   Inputs: 1) pointer to local bit map data (blk_ptr + SIZEOF(blk_hdr)).
           2) bit offset to test into bitmap.
*/
boolean_t gvcst_blk_is_allocated(uchar_ptr_t lbmap, int lm_offset)
{
	int		uchar_offset, uchar_offset_rem;
	unsigned char	result_byte;
	uchar_ptr_t	map_byte;

	assert(BLKS_PER_LMAP * BML_BITS_PER_BLK > lm_offset);
	uchar_offset = lm_offset / BITS_PER_UCHAR;
	uchar_offset_rem = lm_offset % BITS_PER_UCHAR;
	map_byte = lbmap + uchar_offset;
	result_byte = (*map_byte >> uchar_offset_rem) & 0x3;
	switch(result_byte)
	{
		case 0x00:
			return TRUE;
		case 0x01:
		case 0x03:
			return FALSE;
		default:
			GTMASSERT;
	}
	return FALSE; 	/* Can't get here but keep compiler happy */
}

/* Similarly this routine tells if the block was EVER allocated (allocated or recycled) */
boolean_t gvcst_blk_ever_allocated(uchar_ptr_t lbmap, int lm_offset)
{
	int		uchar_offset, uchar_offset_rem;
	unsigned char	result_byte;
	uchar_ptr_t	map_byte;

	assert(BLKS_PER_LMAP * BML_BITS_PER_BLK > lm_offset);
	uchar_offset = lm_offset / BITS_PER_UCHAR;
	uchar_offset_rem = lm_offset % BITS_PER_UCHAR;
	map_byte = lbmap + uchar_offset;
	result_byte = (*map_byte >> uchar_offset_rem) & 0x3;
	switch(result_byte)
	{
		case 0x00:
		case 0x03:
			return TRUE;
		case 0x01:
			return FALSE;
		default:
			GTMASSERT;
	}
	return FALSE; 	/* Can't get here but keep compiler happy */
}
