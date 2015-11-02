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

#include "mdef.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvsub2str.h"
#include "mvalconv.h"
#include "gv_xform_key.h"

GBLREF int4		gv_keysize;
GBLREF gv_namehead	*gv_target;

/* transform gv_currkey or gv_altkey based on collation sequence
 * if XBACK is true then convert from internal to external format.
 * if XBACK is false, convert from external to internal format
 */

void	gv_xform_key(gv_key *keyp,  boolean_t xback)
{
	unsigned char		*c0, *c1, *ctop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_sparekey_size) < gv_keysize)
	{
		if (TREF(gv_sparekey))
			free(TREF(gv_sparekey));
		else
		{
			(TREF(gv_sparekey_mval)).str.addr = (char *)malloc(MAX_ZWR_KEY_SZ);
			(TREF(gv_sparekey_mval)).mvtype = MV_STR;
		}
		TREF(gv_sparekey) = (gv_key *)malloc(SIZEOF(gv_key) - 1 + gv_keysize);
		TREF(gv_sparekey_size) = gv_keysize;
	}
	assert(keyp->top == gv_keysize);
	assert(keyp->end < keyp->top);
	memcpy(TREF(gv_sparekey), keyp, SIZEOF(gv_key) + keyp->end);
	c1 = keyp->base;
	while (*c1++)
		;
	c0 = (TREF(gv_sparekey))->base + (c1 - keyp->base);
	ctop = &((TREF(gv_sparekey))->base[(TREF(gv_sparekey))->end]);
	if (!*c0)		/* no subscipts */
	{
		assert(c0 == ctop);
		return;
	}
	assert(c0 < ctop);
	keyp->prev = 0;
	keyp->end = c1 - keyp->base;
	for (; c0 < ctop; )
	{
		if (STR_SUB_PREFIX != *c0)
		{
			assert(!gv_target->nct);
			while (*c1++ = *c0++)
				;
			keyp->prev = keyp->end;
			keyp->end = c1 - keyp->base;
		} else
		{
			TREF(transform) = xback;
			(TREF(gv_sparekey_mval)).str.len
				= gvsub2str(c0, (unsigned char *)((TREF(gv_sparekey_mval)).str.addr), FALSE)
				- (unsigned char *)(TREF(gv_sparekey_mval)).str.addr;
			TREF(transform) = !xback;
			mval2subsc(TADR(gv_sparekey_mval), keyp);
			c1 = &keyp->base[keyp->end];
			while (*c0++)
				;
		}
		assert(keyp->end < keyp->top);
	}
	return;
}
