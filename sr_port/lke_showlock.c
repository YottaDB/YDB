/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "gtm_string.h"
#include "gtm_stdio.h"		/* for SNPRINTF */

#ifdef VMS
#include <jpidef.h>
#include <ssdef.h>
#include <descrip.h>
#include <syidef.h>
#endif

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "util.h"
#include "lke.h"
#include "is_proc_alive.h"
#include "real_len.h"		/* for real_len() prototype */
#include "zshow.h"
#include "filestruct.h"
#include "mlk_ops.h"

#define	CLNTNODE_LIT	" : CLNTNODE = "
#define CLNTPID_LIT	" : CLNTPID = "
#define	GNAM_FMT_STR	"!AD "
#define LNAM	24
#define NDIM	32		/* max node name size */
#define	PID_FMT_STR	"!UL"
#define	PIDPRINT_LIT	"%d"

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	uint4		process_id;
GBLREF	VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_CTRLC);

#define	NODE_SIZE	64 + 1

void lke_formatlocknode(mlk_shrblk_ptr_t node, mstr* name);
void lke_formatlocknodes(mlk_shrblk_ptr_t node, mstr* name);

static	char	gnam[]    = GNAM_FMT_STR,
		gnaml[]	  = "!AD!/!24*  ",
		ownedby[] = "Owned by PID= " PID_FMT_STR " which is !AD!AD",
		request[] = "Request  PID= " PID_FMT_STR " which is !AD!AD",
		nonexpr[] = "a nonexistent process",
		existpr[] = "an existing process",
		nonexam[] = "an inexaminable process",
		nopriv[]  = "no privilege";

void lke_formatlocknode(mlk_shrblk_ptr_t node, mstr* name)
{
	char			save_ch;
	int			len2;
	mlk_shrsub_ptr_t	value;
	short			len1;
	static mval		subsc = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);

	value = (mlk_shrsub_ptr_t) R2A(node->value);
	if (0 == name->len)
	{
		/* unsubscripted lock name can never have control characters, so no ZWR translation needed */
		memcpy(name->addr, value->data, value->length);
		name->len = value->length;
		name->addr[name->len++] = '(';
	} else
	{
		/* perform ZWR translation on the subscript */
		len1 = name->len - 1;
		if (')' == name->addr[len1])
			name->addr[len1] = ',';
		subsc.str.len = value->length;
		subsc.str.addr = (char*) value->data;
		len2 = MAX_ZWR_KEY_SZ + 1;
		if (val_iscan(&subsc))
		{
			/* avoid printing enclosed quotes for canonical numbers */
			save_ch = name->addr[len1];
			format2zwr((sm_uc_ptr_t) value->data, value->length, (unsigned char*) &name->addr[len1], &len2);
			assert(name->addr[len1 + len2 - 1] == '"');
			name->addr[len1] = save_ch;
			len2 -= 2; /* exclude the enclosing quotes */
		}
		else
			format2zwr((sm_uc_ptr_t) value->data, value->length, (unsigned char*) &name->addr[name->len], &len2);

		name->len += len2;
		name->addr[name->len++] = ')';
	}
}

void lke_formatlocknodes(mlk_shrblk_ptr_t node, mstr* name)
{
	if (node->parent)
		lke_formatlocknodes((mlk_shrblk_ptr_t)R2A(node->parent), name);
	lke_formatlocknode(node, name);
}

void lke_formatlockname(mlk_shrblk_ptr_t node, mstr* name)
{
	lke_formatlocknodes(node, name);
	if ('(' == name->addr[name->len - 1])
		name->len--;
}

bool	lke_showlock(mlk_pvtctl_ptr_t	pctl,
		     struct CLB		*lnk,
		     mlk_shrblk_ptr_t	node,
		     mstr		*name,
		     bool		all,
		     bool		wait,
		     bool		interactive,
		     int4 		pid,
		     mstr		one_lock,
		     boolean_t		exact,
		     intrpt_state_t	*prev_intrpt_state)
{
	boolean_t		lock = FALSE, owned, unsub, was_crit = FALSE;
	char			format[NODE_SIZE], gtcmbuf[NODE_SIZE], *msg;
	int 			len2;
	int4			gtcmbufidx;
	mlk_prcblk		pblk;
	mlk_prcblk_ptr_t	our;
	short			len1;
	UINTPTR_T		f[7];

	unsub = (0 == name->len);
	lke_formatlocknode(node, name);
	f[0] = name->len + (unsub ? -1 : 0);		/* Subtract one for the lparen in unsubscripted name */
	f[1] = (UINTPTR_T)name->addr;
	if (node->owner || (node->pending && wait))
	{
		pblk.process_id = node->owner;
		pblk.process_start = node->pstart;
		pblk.next = (wait && node->pending) ?
			(ptroff_t)((uchar_ptr_t)&node->pending - (uchar_ptr_t)&pblk.next + node->pending)
						       : 0;
		owned = (all || !wait) && node->owner;
		our = owned ? &pblk
			  : ((0 == node->pending) ? NULL
						: (mlk_prcblk_ptr_t)R2A(node->pending));
		while (NULL != our)
		{
			if ((0 == pid) || (pid == our->process_id))
			{
				f[2] = our->process_id;
				if (is_proc_alive((int4)our->process_id, (int4)our->process_start))
				{
					f[3] = STR_LIT_LEN(existpr);
					f[4] = (INTPTR_T)existpr;
				} else
				{
					f[3] = STR_LIT_LEN(nonexpr);
					f[4] = (UINTPTR_T)nonexpr;
				}
				if (node->auxowner)
				{
					gtcmbufidx = 0;
					memcpy(&gtcmbuf[gtcmbufidx], CLNTNODE_LIT, STR_LIT_LEN(CLNTNODE_LIT));
					gtcmbufidx += STR_LIT_LEN(CLNTNODE_LIT);
					memcpy(&gtcmbuf[gtcmbufidx], node->auxnode, SIZEOF(node->auxnode));
					gtcmbufidx += real_len(SIZEOF(node->auxnode), (uchar_ptr_t)node->auxnode);
					memcpy(&gtcmbuf[gtcmbufidx], CLNTPID_LIT, STR_LIT_LEN(CLNTPID_LIT));
					gtcmbufidx += STR_LIT_LEN(CLNTPID_LIT);
					SNPRINTF(&gtcmbuf[gtcmbufidx], NODE_SIZE - gtcmbufidx, PIDPRINT_LIT, node->auxpid);
					f[5] = strlen(gtcmbuf);
					f[6] = (UINTPTR_T)&gtcmbuf[0];
					assert(f[5] > gtcmbufidx);
					assert(gtcmbufidx < SIZEOF(gtcmbuf));
				} else
					f[5] = f[6] = 0;
				if (interactive)
				{
					if (LNAM >= f[0])
					{
						msg = gnam;
						len1 = STR_LIT_LEN(gnam);
					} else
					{
						msg = gnaml;
						len1 = STR_LIT_LEN(gnaml);
					}
					memcpy(format, msg, len1);

					if (owned && !lock)
					{
						msg = ownedby;
						len2 = STR_LIT_LEN(ownedby);
					} else
					{
						msg = request;
						len2 = STR_LIT_LEN(request);
					}
					memcpy(format + len1, msg, len2);
					format[len1 + len2] = '\0';
					assert((len1 + len2) < SIZEOF(format));
					if (NULL == lnk)
					{
						if ((NULL == one_lock.addr) ||
							(!memcmp(name->addr, one_lock.addr, one_lock.len)
							&& (!exact || (one_lock.len == f[0]))))
							util_out_print_args(format, 7, FLUSH,
									f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
					} else
						util_cm_print(lnk, CMMS_V_LKESHOW, format, 7, FLUSH,
							      f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
				}
				if ((NULL != one_lock.addr) &&
					(memcmp(name->addr, one_lock.addr, one_lock.len) ||
					(exact && (one_lock.len != f[0]))))
				{
					lock = FALSE;
					return lock;
				}
				lock = TRUE;
				if (util_interrupt)
				{
					if (LOCK_CRIT_HELD(pctl->csa))
						REL_LOCK_CRIT(pctl, was_crit);
					if (NULL != prev_intrpt_state)
						ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, *prev_intrpt_state);
					rts_error_csa(CSA_ARG(pctl->csa) VARLSTCNT(1) ERR_CTRLC);
				}
			}
			f[0] = 0;
			our = (0 == our->next) ? (mlk_prcblk_ptr_t)NULL : (mlk_prcblk_ptr_t)R2A(our->next);
		}
	}
	return lock;
}
