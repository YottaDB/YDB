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

GBLREF bool		transform;
GBLREF int4		gv_keysize;
GBLREF gv_namehead	*gv_target;

/* transform gv_currkey or gv_altkey based on collation sequence
 * if XBACK is true then convert from internal to external format.
 * if XBACK is false, convert from external to internal format
 */

void	gv_xform_key(gv_key *keyp, bool xback)
{
	static int4		gv_sparekey_size  = 0;
	static gv_key		*gv_sparekey = NULL;
	static mval		temp;
	static unsigned char	buff[MAX_ZWR_KEY_SZ];
	unsigned char		*c0, *c1, *ctop;

	if (gv_sparekey_size < gv_keysize)
	{
		if (gv_sparekey)
			free(gv_sparekey);
		else
		{
			temp.str.addr = (char *)buff;
			temp.mvtype = MV_STR;
		}
		gv_sparekey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
		gv_sparekey_size = gv_keysize;
	}
	assert(keyp->top == gv_keysize);
	assert(keyp->end < keyp->top);
	memcpy(gv_sparekey, keyp, sizeof(gv_key) + keyp->end);
	c1 = keyp->base;
	while (*c1++)
		;
	c0 = gv_sparekey->base + (c1 - keyp->base);
	ctop = &gv_sparekey->base[gv_sparekey->end];
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
			transform = xback;
			temp.str.len = INTCAST(gvsub2str(c0, buff, FALSE) - buff);
			transform = !xback;
			mval2subsc(&temp, keyp);
			c1 = &keyp->base[keyp->end];
			while (*c0++)
				;
		}
		assert(keyp->end < keyp->top);
	}
	return;
}
