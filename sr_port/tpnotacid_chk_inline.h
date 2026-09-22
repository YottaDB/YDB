/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TPNOTACID_CHK_INLINE_INCLUDED
#define TPNOTACID_CHK_INLINE_INCLUDED

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_common_defs.h"
#include "gdsbt.h"
#include "gtm_fcntl.h"	/* Needed for AIX's silly open to open64 translations */
#include "gdsfhead.h"
#include "filestruct.h"
#include "stringpool.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "getzposition.h"
#include "send_msg.h"
#include "iotimer.h"
#include "is_equ.h"		/* for "mv_force_nstimeout" below */
#include "mvalconv.h"
#include "min_max.h"
#ifdef DEBUG
#include "change_reg.h"
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

#define IS_TP_AND_FINAL_RETRY  (dollar_tlevel && (CDB_STAGNATE <= t_tries))
#define TPNOTACID_DEFAULT_TIME	2000	/* default (in milliseconds) for tpnotacidtime */
#define TPNOTACID_MAX_TIME	30000	/* maximum (in milliseconds) for tpnotacidtime */
#define TPTIMEOUT_MAX_TIME	60000	/* maximum (in milliseconds) for dollar_zmaxtptime */
#define TPNOTACID_DEF_MAX_TRIES	16	/* default TPNOTACID tries */
#define TPNOTACID_LIM_MAX_TRIES	100	/* limit on TPNOTACID tries */
#define NOTPNOTACID		"NA"	/* for users of MV_FORCE_NSTIMEOUT to supress TPNOACID check, e.g. ztimeout_routines */

GBLREF	boolean_t	mupip_jnl_recover;
GBLREF	tp_region	*tp_reg_list;
GBLREF	unsigned int	t_tries;
GBLREF	uint4		dollar_tlevel, dollar_trestart;

LITREF	mval		literal_notimeout;

error_def(ERR_TPNOTACID);

#define	TP_REL_CRIT_ALL_REG tp_rel_crit_all_reg()

static inline void tp_rel_crit_all_reg()
{
	sgmnt_addrs	*csa;
	tp_region	*tr;

	for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
	{
		assert(tr->reg->open);
		if (!tr->reg->open)
			continue;
		csa = (sgmnt_addrs *)&FILE_INFO(tr->reg)->s_addrs;
		assert(!csa->hold_onto_crit);
		if (csa->now_crit)
			rel_crit(tr->reg);
	}
	assert(!have_crit(CRIT_HAVE_ANY_REG));
}

#define TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK									\
MBSTART {													\
	GBLREF	boolean_t	mupip_jnl_recover;								\
														\
	assert(dollar_tlevel);											\
	assert(CDB_STAGNATE == t_tries);									\
	/* mupip_jnl_recovery operates with t_tries=CDB_STAGNATE so we should not adjust t_tries		\
	 * In that case, because we have standalone access, we don't expect anyone else to interfere with us 	\
	 * and cause a restart, but if they do, the TPNOTACID_CHECK (below) gives a TPNOTACID message.		\
	 */													\
	if (!mupip_jnl_recover)											\
	{													\
		assert((int4)dollar_trestart >= TREF(tp_restart_dont_counts));					\
		t_tries = CDB_STAGNATE - 1;									\
		DEBUG_ONLY(if (0 == TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts))++);	/* can live with one too many */	\
		DEBUG_ONLY(if (0 < TREF(tp_restart_dont_counts)))						\
			DEBUG_ONLY((TREF(tp_restart_dont_counts)) = -(TREF(tp_restart_dont_counts)));		\
	}													\
} MBEND

#define	TPNOTACID_CHECK(CALLER_STR) if (dollar_tlevel) tpnotacid_check(CALLER_STR);

STATICDEF char	*last_caller, *last_zpos_addr;

static inline void tpnotacid_check(char* caller_str)
{
	mval	zpos;
	uint4	cslen, tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(zpos.str.addr = NULL);
	assert(dollar_tlevel);
	if ((tries = TREF(tpnotacidtries)) && !dollar_trestart)			/* WARNING: assignment */
		return;								/* 1st try gets a pass except under "audit" */
	if (CDB_STAGNATE <= t_tries)
	{	/* Note: The TP_REL_CRIT_ALL_REG call needs to happen AFTER the TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK
		 * call as otherwise it is possible a SIGTERM/SIGINT/SIGQUIT signal gets handled as part of the
		 * TP_REL_CRIT_ALL_REG call taking us to exit processing while "t_tries" is still set to "CDB_STAGNATE"
		 * which would confuse other code as we are effectively in the final retry but not holding crit on a
		 * region inside a TP transaction.
		 */
		TP_FINAL_RETRY_DECREMENT_T_TRIES_IF_OK;
		TP_REL_CRIT_ALL_REG;
	}
	if ((CDB_STAGNATE <= dollar_trestart) || !tries)
	{	/* syslog message treats 0 == tries like an "audit" request */
		getzposition(&zpos);
		if ((last_zpos_addr != zpos.str.addr) && (last_caller != caller_str))
		{	/* nag limited to change from last one */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TPNOTACID, 6, LEN_AND_STR(caller_str), zpos.str.len,
				zpos.str.addr, TREF(tpnotacidtime), dollar_trestart);
			last_zpos_addr = zpos.str.addr;
			last_caller = caller_str;
		}	/* send_msg might get duplicated but not considered likely or a problem */
		if (dollar_trestart > (tries ? tries : TPNOTACID_DEF_MAX_TRIES))
		{	/* hard limit; tries rarely zero as that is intended as a way of discovering not acid code */
			assert(zpos.str.addr);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) MAKE_MSG_ERROR(ERR_TPNOTACID), 6, LEN_AND_STR(caller_str),
				zpos.str.len, zpos.str.addr, TREF(tpnotacidtime), dollar_trestart);
			cslen = strlen(caller_str);	/* for reasons not identified 2nd LEN_AND_STR produced unreliable length */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) MAKE_MSG_ERROR(ERR_TPNOTACID), 6, cslen, caller_str, zpos.str.len,
				zpos.str.addr, TREF(tpnotacidtime), dollar_trestart);
		}
	}
}

#define MV_FORCE_NSTIMEOUT(TMV, TNS, NOTACID) mv_force_nstimeout(TMV, TNS, NOTACID)

/* YottaDB arms its timers in nanoseconds ("start_timer" takes a uint8 and ABS_TIME is a "struct timespec"),
 * so this is the nanosecond counterpart of GT.M's "mv_force_mstimeout". Two differences follow from that:
 *   1. NO_M_TIMEOUT is a 64-bit sentinel in YottaDB (sr_port/iotimer.h) and so does not fit in "tmv->m[1]".
 *      The no-timeout test therefore compares against "literal_notimeout" rather than against "m[1]".
 *   2. A positive timeout is capped at MAXUINT8 rather than MAXPOSINT4.
 * TREF(tpnotacidtime) is a millisecond count in both trees, so it is scaled up for the comparison below.
 */
static inline void mv_force_nstimeout(mval *tmv, uint8 *tns, char *notacid)
{
	double		tmpdouble;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_NUM(tmv);
	if (is_equ((mval *)&literal_notimeout, tmv))
		*tns = NO_M_TIMEOUT;
	else
	{
		assert(MV_BIAS == MILLISECS_IN_SEC);	/* if formats change scale may need attention */
		if (tmv->mvtype & MV_INT)
		{	/* tmv is an integer. m[1] directly has the # of milliseconds we want.
			 * If it is negative though, set timeout to 0.
			 */
			*tns = tmv->m[1] * (uint8)NANOSECS_IN_MSEC;
			if (0 > *tns)
				*tns = 0;
		} else if (0 == tmv->sgn)	/* if sign is 0 it means tmv is positive */
		{	/* Cap positive timeout at MAXUINT8 */
			tmpdouble = mval2double(tmv) * (double)NANOSECS_IN_SEC;
			*tns = ((double)MAXUINT8 >= tmpdouble) ? (uint8)tmpdouble : (uint8)MAXUINT8;
		} else
			*tns = 0;		/* sign is not zero, implies tmv is negative, set timeout to 0 */
	}
	/* no TPNOTACID for ZTIMEOUT */
	if (((TREF(tpnotacidtime) * (uint8)NANOSECS_IN_MSEC) < *tns) && (STRNCMP_LIT(notacid, NOTPNOTACID)))
		TPNOTACID_CHECK(notacid);
}
#endif /* TPNOTACID_CHK_INLINE_INCLUDED */
