/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZHIST_H_INCLUDED
#define ZHIST_H_INCLUDED

#include "relinkctl.h"
#include "zroutinessp.h"

/*
 * Suppose, for example, $ZROUTINES="dir1 dir2 dir3".
 * TODO: explain validation logic............
 */

typedef struct
{
	uint4			cycle;				/* private copy */
	relinkrec_loc_t		cycle_loc;			/* location of record containing shared copy */
	open_relinkctl_sgm	*relinkctl_bkptr;
	/* backpointer to zro_ent? any reason for that? maybe debugging? */
} zro_validation_entry;

typedef struct
{
	uint4			zroutines_cycle;		/* compare to set_zroutines_cycle */
	zro_validation_entry	*end;				/* end - &base[0] = size of allocated validation array */
	zro_validation_entry	base[1];			/* base of allocated validation array */
} zro_hist;

boolean_t need_relink(rhdtyp *rtnhdr, zro_hist *zhist);
void zro_zhist_saverecent(zro_validation_entry *zhent, zro_validation_entry *zhent_base);
void zro_record_zhist(zro_validation_entry *zhent, zro_ent *obj_container, mstr *rtnname);

#endif /* ZHIST_H_INCLUDED */
