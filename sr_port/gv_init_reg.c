/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "iosp.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cryptdef.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "gvusr.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */

GBLREF int4		lkid;
GBLREF bool		licensed ;
GBLREF int4		gv_keysize;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region	*gv_cur_region;

void gv_init_reg (gd_region *reg)
{
	gv_key		*temp_key;
	gv_namehead	*g;
	sgmnt_addrs	*csa;
	int		keysize;
#ifdef	NOLICENSE
	licensed= TRUE ;
#else
	CRYPT_CHKSYSTEM ;
#endif
	switch (reg->dyn.addr->acc_meth)
	{
	case dba_usr:
		gvusr_init (reg, &gv_cur_region, &gv_currkey, &gv_altkey);
		break;
		/* we may be left in dba_cm state for gt_cm, if we have rundown the db and again accessed
		   the db without quitting out of gtm */
	case dba_cm:
	case dba_mm:
	case dba_bg:
		if( FALSE == reg->open)
		  gvcst_init (reg);
		break;
	default:
		GTMASSERT;
	}
	assert(reg->open);

	keysize = (reg->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);

	if (keysize > gv_keysize)
	{
		gv_keysize = keysize;
		temp_key = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
		if (gv_currkey)
		{
			memcpy(temp_key, gv_currkey, sizeof(gv_key) + gv_currkey->end);
			free(gv_currkey);
		} else
			temp_key->base[0] = '\0';
		gv_currkey = temp_key;
		gv_currkey->top = gv_keysize;
		temp_key = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
		if (gv_altkey)
		{
			memcpy(temp_key, gv_altkey, sizeof(gv_key) + gv_altkey->end);
			free(gv_altkey);
		} else
			temp_key->base[0] = '\0';
		gv_altkey = temp_key;
		gv_altkey->top = gv_keysize;
	}
	if (reg->dyn.addr->acc_meth == dba_bg || reg->dyn.addr->acc_meth == dba_mm)
	{
		if (!reg->was_open)
		{
			csa = (sgmnt_addrs*)&FILE_INFO(reg)->s_addrs;
			g = csa->dir_tree;
			if (NULL != g)
			{	/* It is possible that dir_tree has already been targ_alloc'ed. This is because GT.CM or VMS DAL
				 * calls can run down regions without the process halting out. We don't want to double malloc.
				 */
				g->clue.end = 0;
			}
			SET_CSA_DIR_TREE(csa, reg->max_key_size, reg);
		}
	}
	return;
}
