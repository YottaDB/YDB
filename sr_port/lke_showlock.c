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

/*
 * ----------------------------------------------------------------
 * lke_showlock : displays the lock data for a given lock tree node
 * used in	: lke_showtree.c
 * ----------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"		/* for SPRINTF */

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

#define LNAM		24
#define NDIM		32		/* max node name size */
#define	CLNTNODE_LIT	" : CLNTNODE = "
#define CLNTPID_LIT	" : CLNTPID = "

#if defined(UNIX)
#	define	PID_FMT_STR	"!UL"
#	define	PIDPRINT_LIT	"%d"
#	define	GNAM_FMT_STR	"!AD "
#elif defined(VMS)
#	define	PID_FMT_STR	"!XL"
#	define	PIDPRINT_LIT	"%08X"
#	define	GNAM_FMT_STR	"!24<!AD!> "
#endif

GBLREF	int4		process_id;

static	char	gnam[]    = GNAM_FMT_STR,
		gnaml[]	  = "!AD!/!24*  ",
		ownedby[] = "Owned by PID= " PID_FMT_STR " which is !AD!AD",
		request[] = "Request  PID= " PID_FMT_STR " which is !AD!AD",
		nonexpr[] = "a nonexistent process",
		existpr[] = "an existing process",
		nonexam[] = "an inexaminable process",
		nopriv[]  = "no privilege";

bool	lke_showlock(
		     struct CLB		*lnk,
		     mlk_shrblk_ptr_t	tree,
		     mstr		*name,
		     bool		all,
		     bool		wait,
		     bool		interactive,
		     int4 		pid,
		     mstr		one_lock,
		     boolean_t		exact)
{
	mlk_prcblk	pblk;
	mlk_prcblk_ptr_t r;
	mlk_shrsub_ptr_t value;
	short		len1;
	int 		len2;
	boolean_t	lock = FALSE, owned;
	UINTPTR_T	f[7];
        int4            gtcmbufidx, item, ret;
	uint4		status;
	char		*msg, save_ch, format[64], gtcmbuf[64];	/* gtcmbuf[] is to hold ": CLNTNODE = %s : CLNTPID = %s" */
	static	mval	subsc = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);
	VMS_ONLY(
		char		sysinfo[NDIM];
		$DESCRIPTOR	(sysinfo_dsc, sysinfo);
	)

	value = (mlk_shrsub_ptr_t)R2A(tree->value);
	if (0 == name->len)
	{ /* unsubscripted lock name can never have control characters, so no ZWR translation needed */
		memcpy(name->addr, value->data, value->length);
		f[0] = name->len = value->length;
		name->addr[name->len++] = '(';
	} else
	{ /* perform ZWR translation on the subscript */
		len1 = name->len - 1;
		if (')' == name->addr[len1])
			name->addr[len1] = ',';
		subsc.str.len = value->length;
		subsc.str.addr = (char *)value->data;
		if (val_iscan(&subsc))
		{ /* avoid printing enclosed quotes for canonical numbers */
			save_ch = name->addr[len1];
			format2zwr((sm_uc_ptr_t)value->data, value->length, (unsigned char*)&name->addr[len1], &len2);
			assert(name->addr[len1 + len2 - 1] == '"');
			name->addr[len1] = save_ch;
			len2 -= 2; /* exclude the enclosing quotes */
		} else
			format2zwr((sm_uc_ptr_t)value->data, value->length, (unsigned char*)&name->addr[name->len], &len2);
		name->len += len2;
		name->addr[name->len++] = ')';
		f[0] = name->len;
	}
	f[1] = (UINTPTR_T)name->addr;
	if (tree->owner || (tree->pending && wait))
	{
		pblk.process_id = tree->owner;
		pblk.next = (wait && tree->pending) ?
			(ptroff_t)((uchar_ptr_t)&tree->pending - (uchar_ptr_t)&pblk.next + tree->pending)
						       : 0;
		owned = (all || !wait) && tree->owner;
		r = owned ? &pblk
			  : ((0 == tree->pending) ? NULL
						: (mlk_prcblk_ptr_t)R2A(tree->pending));
		while (NULL != r)
		{
			if ((0 == pid) || (pid == r->process_id))
			{
				f[2] = r->process_id;
				VMS_ONLY(
					item = JPI$_STATE;
					status = lib$getjpi(&item, &r->process_id, 0, &ret, &sysinfo_dsc, &len1);
					switch (status)
					{
					case SS$_NORMAL:
						f[3] = len1;
						f[4] = sysinfo;
						break;
					case SS$_NOPRIV:
						f[3] = STR_LIT_LEN(nopriv);
						f[4] = nopriv;
						break;
					case SS$_NONEXPR:
						f[3] = STR_LIT_LEN(nonexpr);
						f[4] = nonexpr;
						break;
					default:
						f[3] = STR_LIT_LEN(nonexam);
						f[4] = nonexam;
						break;
					}
				)
				UNIX_ONLY(
					if (is_proc_alive((int4)r->process_id, 0))
					{
						f[3] = STR_LIT_LEN(existpr);
						f[4] = (INTPTR_T)existpr;
					} else
					{
						f[3] = STR_LIT_LEN(nonexpr);
						f[4] = (UINTPTR_T)nonexpr;
					}
				)
				if (tree->auxowner)
				{
					gtcmbufidx = 0;
					memcpy(&gtcmbuf[gtcmbufidx], CLNTNODE_LIT, STR_LIT_LEN(CLNTNODE_LIT));
					gtcmbufidx += STR_LIT_LEN(CLNTNODE_LIT);
					memcpy(&gtcmbuf[gtcmbufidx], tree->auxnode, SIZEOF(tree->auxnode));
					gtcmbufidx += real_len(SIZEOF(tree->auxnode), (uchar_ptr_t)tree->auxnode);
					memcpy(&gtcmbuf[gtcmbufidx], CLNTPID_LIT, STR_LIT_LEN(CLNTPID_LIT));
					gtcmbufidx += STR_LIT_LEN(CLNTPID_LIT);
					SPRINTF(&gtcmbuf[gtcmbufidx], PIDPRINT_LIT, tree->auxpid);
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
							util_out_print(format, FLUSH, f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
					} else
						util_cm_print(lnk, CMMS_V_LKESHOW, format, FLUSH,
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
			}
			f[0] = 0;
			r = (0 == r->next) ? (mlk_prcblk_ptr_t)NULL : (mlk_prcblk_ptr_t)R2A(r->next);
		}
	}
	return lock;
}
