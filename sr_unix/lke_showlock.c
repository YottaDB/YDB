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

/*
 * ----------------------------------------------------------------
 * lke_showlock : displays the lock data for a given lock tree node
 * used in	: lke_showtree.c
 * ----------------------------------------------------------------
 */

#include "mdef.h"

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashdef.h"
#include "cmmdef.h"
#include "util.h"
#include "is_proc_alive.h"
#include "lke.h"

#define FLUSH		1
#define LNAM		24
#define SDIM		256		/* max subscript size */

GBLREF	int4	process_id;

static	char	gnam[]	  = "!AD ",
		gnaml[]	  = "!AD!/!24*  ",
		ownedby[] = "Owned by PID= !UL which is !AD!AD",
		request[] = "Request  PID= !UL which is !AD!AD",
		nonexpr[] = "a nonexistent process",
		existpr[] = "an existing process";


bool	lke_showlock(
		     struct CLB		*lnk,
		     mlk_shrblk_ptr_t	tree,
		     mstr		*name,
		     bool		all,
		     bool		wait,
		     bool		interactive,
		     int4 		pid,
		     mstr		one_lock)
{
	mlk_prcblk	pblk;
	mlk_prcblk_ptr_t r;
	mlk_shrsub_ptr_t value;
	short		len1, len2;
	bool		lock = FALSE, owned;
	int4		f[7];
	char		*msg, format[64];


	value = (mlk_shrsub_ptr_t)R2A(tree->value);
	if (name->len == 0)
	{
		memcpy(name->addr, value->data, value->length);
		f[0] = name->len
		     = value->length;
		name->addr[name->len++] = '(';
	}
	else
	{
		len1 = name->len - 1;
		if (name->addr[len1] == ')')
			name->addr[len1] = ',';
		memcpy(&name->addr[name->len], value->data, value->length);
		name->len += value->length;
		name->addr[name->len++] = ')';
		f[0] = name->len;
	}
	f[1] = (int4)name->addr;

	if (tree->owner != 0  ||  (tree->pending != 0  &&  wait))
	{
		pblk.process_id = tree->owner;
		pblk.next = wait && tree->pending != 0 ? ((uchar_ptr_t)&tree->pending - (uchar_ptr_t)&pblk.next + tree->pending)
						       : 0;
		owned = (all || !wait) && tree->owner != 0;
		r = owned ? &pblk
			  : (tree->pending == 0 ? NULL
						: (mlk_prcblk_ptr_t)R2A(tree->pending));

		while (r != NULL)
		{
			if (pid == 0  ||  pid == r->process_id)
			{
				f[2] = r->process_id;

				if (is_proc_alive((int4)r->process_id, 0))
				{
					f[3] = sizeof existpr - 1;
					f[4] = (int4)existpr;
				}
				else
				{
					f[3] = sizeof nonexpr - 1;
					f[4] = (int4)nonexpr;
				}
				f[5] = f[6] = 0;

				if (interactive)
				{
					if (f[0] <= LNAM)
					{
						msg = gnam;
						len1 = sizeof gnam - 1;
					}
					else
					{
						msg = gnaml;
						len1 = sizeof gnaml - 1;
					}
					memcpy(format, msg, len1);

					if (owned  &&  !lock)
					{
						msg = ownedby;
						len2 = sizeof ownedby - 1;
					}
					else
					{
						msg = request;
						len2 = sizeof request - 1;
					}
					memcpy(format + len1, msg, len2);
					format[len1 + len2] = '\0';

					if (lnk == NULL)
					{
						if ((memcmp(name->addr, one_lock.addr, one_lock.len) == 0) ||
								(one_lock.addr == NULL))
							util_out_print(format, FLUSH, f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
					}
					else
						util_cm_print(lnk, CMMS_V_LKESHOW, format, FLUSH,
							      f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
				}
				if ((memcmp(name->addr, one_lock.addr, one_lock.len) != 0) && (one_lock.addr != NULL))
				{
					lock = FALSE;
					return lock;
				}
				lock = TRUE;
			}

			f[0] = 0;
			r = r->next == 0 ? (mlk_prcblk_ptr_t)NULL
					 : (mlk_prcblk_ptr_t)R2A(r->next);
		}
	}

	return lock;
}
