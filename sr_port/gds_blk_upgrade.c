/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
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
#include "v15_gdsroot.h"
#include "gdsblk.h"
#include "gdsdbver.h"
#include "gds_blk_upgrade.h"
#include "iosp.h"
#include "copy.h"

#define SPACE_NEEDED (SIZEOF(blk_hdr) - SIZEOF(v15_blk_hdr))

GBLREF	boolean_t	gtm_blkupgrade_override;
GBLREF	uint4		gtm_blkupgrade_flag;	/* control whether dynamic upgrade is attempted or not */

error_def(ERR_DYNUPGRDFAIL);

int4 gds_blk_upgrade(sm_uc_ptr_t gds_blk_src, sm_uc_ptr_t gds_blk_trg, int4 blksize, enum db_ver *ondsk_blkver)
{
	blk_hdr_ptr_t		bp;
	v15_blk_hdr_ptr_t	v15bp;
	v15_trans_num		v15tn;
	uint4			v15bsiz, v15levl;

	assert(gds_blk_src);
	assert(gds_blk_trg);
	/* Assert that the input buffer is 8-byte aligned for us to freely de-reference 8-byte tn fields from the buffer header.
	 * If not, we should have had to use GET/PUT_xxxx macros from copy.
	 * Note that in GDSV4 format in VMS, the "tn" field in the block-header is not 4-byte offset aligned so we
	 *	need to use the GET_ULONG macro to fetch the field from the block header. But since "tn" is 8-byte
	 *	offset aligned in the new format, there is no need of any such macro while assigning bp->tn.
	 */
	assert(0 == ((long)gds_blk_src & 0x7));	/* Assume 8 byte alignment */
	assert(0 == ((long)gds_blk_trg & 0x7));
	v15bp = (v15_blk_hdr_ptr_t)gds_blk_src;
	bp = (blk_hdr_ptr_t)gds_blk_trg;
	assert((SIZEOF(v15_blk_hdr) <= v15bp->bsiz) || (UPGRADE_ALWAYS == gtm_blkupgrade_flag));
	UNIX_ONLY(v15tn = v15bp->tn);
	VMS_ONLY(GET_ULONG(v15tn, &v15bp->tn));
	v15bsiz = v15bp->bsiz;
	if (v15bsiz > blksize) /* Exceeds maximum block size. Not a valid V4 block. Return without upgrading */
	{
		assert(UPGRADE_NEVER != gtm_blkupgrade_flag);
		if (UPGRADE_IF_NEEDED == gtm_blkupgrade_flag)
		{
			if (NULL != ondsk_blkver)
				*ondsk_blkver = GDSV6;
			return SS_NORMAL;
		} else
		{
			if (NULL != ondsk_blkver)
				*ondsk_blkver = GDSV4;
			return ERR_DYNUPGRDFAIL;
		}
	}
	if (NULL != ondsk_blkver)
		*ondsk_blkver = GDSV4;
	v15bsiz += SPACE_NEEDED;
	if (v15bsiz > blksize) /* Exceeds maximum block size */
		return ERR_DYNUPGRDFAIL;
	v15levl = v15bp->levl;
	memmove((gds_blk_trg + SPACE_NEEDED), gds_blk_src, v15bp->bsiz);	/* Shift/copy block requisite amount */
	bp->tn = v15tn;
	bp->bsiz = v15bsiz;
	bp->levl = v15levl;
	bp->bver = GDSV6;
	return SS_NORMAL;
}
