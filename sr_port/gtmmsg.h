/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMMSG_H_INCLUDED
#define GTMMSG_H_INCLUDED

#ifdef VMS
void gtm_getmsg(uint4 msgnum, mstr *msgbuf);
void gtm_putmsg(int4 msgid, ...);
#define gtm_putmsg_csa	gtm_putmsg
#elif defined(UNIX)
void gtm_getmsg(int4 msgnum, mstr *msgbuf);
void gtm_putmsg(int argcnt, ...);
void gtm_putmsg_csa(void *, int argcnt, ...);		/* Use CSA_ARG(CSA) for portability */
void gtm_putmsg_noflush(int argcnt, ...);
void gtm_putmsg_noflush_csa(void *, int argcnt, ...);

GBLREF	boolean_t	multi_thread_in_use;		/* TRUE => threads are in use. FALSE => not in use */

/* If threads are in use, then do not use "gv_cur_region" as the thread could be operating on a region completely different
 * from the process-wide "gv_cur_region" variable. Assume a safe value of NULL as the csa.
 */
#define	PTHREAD_CSA_FROM_GV_CUR_REGION ((CUSTOM_ERRORS_LOADED && !multi_thread_in_use) ? REG2CSA(gv_cur_region) : NULL)

# define GET_MSG_IDX(MSG_ID, CTL, IDX)										\
{														\
	assert(NULL != CTL);											\
	assert((MSG_ID && FACMASK(CTL->facnum)) && (MSGMASK(MSG_ID, CTL->facnum) <= CTL->msg_cnt));		\
	IDX = MSGMASK(MSG_ID, CTL->facnum) - 1;									\
}

/* Given a pointer to ctl array (merrors_ctl, gdeerrors_ctl, etc.) and a msg_id, get the structure corresponding to that msg_id */
# define GET_MSG_INFO(MSG_ID, CTL, MSG_INFO)									\
{														\
	int			idx;										\
														\
	GET_MSG_IDX(MSG_ID, CTL, idx);										\
	MSG_INFO = CTL->fst_msg + idx;										\
}

#else
# error Unsupported platform
#endif

#endif /* GTMMSG_H_INCLUDED */
