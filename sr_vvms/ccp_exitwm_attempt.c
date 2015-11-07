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

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include <ssdef.h>


/********************************************************************************
*   After any of the state transitions which might lead to exit write mode	*
*   processing, this routine must be called to determine if all necessary	*
*   criteria have been met.							*
*										*
*   CAUTION: (1) To avoid race conditions this routine MUST only be called as	*
*		 an AST;							*
*	     (2) All bit fiddling with the db_header state bits MUST be done	*
*		 within an AST for the same reason.				*
********************************************************************************/

void ccp_exitwm_attempt( ccp_db_header	*db)
{
	ccp_action_record	request;
	int4			curr_time[2], result_time[2], ticks_left;
	uint4		status;


	assert(lib$ast_in_prog());

	if (db->quantum_expired  &&  db->wmexit_requested  &&
	    db->segment != NULL  &&  db->segment->nl->ccp_state == CCST_DRTGNT)
	{
		/* If first request is after quantum expired, one extra tick to try and get writes off */
		if (db->drop_lvl == 0)
		{
			if (!db->extra_tick_started)
			{
				/* db->glob_sec->wcs_active_lvl -= db->glob_sec->n_bts / 2 - 7; */
				db->extra_tick_started = TRUE;
				status = sys$setimr(0, &db->glob_sec->ccp_tick_interval, ccp_extra_tick, &db->extra_tick_id, 0);
				if (status == SS$_NORMAL)
					return;
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			}
			else
				if (!db->extra_tick_done)
					return;
			db->extra_tick_started = db->extra_tick_done
					       = FALSE;
		}
		assert(!db->tick_in_progress);
		db->segment->nl->ccp_crit_blocked = TRUE;
		if (db->segment->nl->in_crit == 0  &&  db->glob_sec->freeze == 0)
		{
			request.action = CCTR_EXITWM;
			request.pid = 0;
			request.v.exreq.fid = FILE_INFO(db->greg)->file_id;
			request.v.exreq.cycle = db->segment->nl->ccp_cycle;
			ccp_act_request(&request);
		}
	}
	else
		if (!db->quantum_expired  &&  db->wmexit_requested  &&  db->drop_lvl == 0)
		{
			/* Gradually drop active_lvl to reduce the number of dirty
			   buffers the CCP has to write upon releasing write mode */
			sys$gettim(curr_time);
			lib$sub_times(&db->start_wm_time, curr_time, result_time);
			ticks_left = result_time[0] / db->glob_sec->ccp_tick_interval[0];
			if (ticks_left >= 1)
				db->drop_lvl = (db->glob_sec->n_bts / 2 - 7) / ticks_left;
		}

	return;
}

/***********************************************************************
Write mode may not be exited while a GT.M process is processing in the critical
section.  In order to ensure this, the following scheme is used:

THIS ROUTINE:
 1. crit_blocked <-- 1
 2. if crit is not owned then raise request for CCTR_EXITWM

GRAB_CRIT:
 1. Get critical section
 2. If crit_blocked is set
	A. Raise request for CCTR_EXITWM, wait for WMEXIT to complete
	B. If not write mode, request write mode, wait to enter write mode

REL_CRIT:
 1. Acquire cycle number
 1. Release critical section
 2. If crit_blocked is set
	A. Raise request for CCTR_EXITWM for cycle number,
		wait for WMEXIT to complete

Requests for CCTR_EXITWM include the cycle number.  If duplicates or 'obsolete'
requests are received, they are ignored.  For duplicate requests, i.e., the CCP
is currently in the process of releasing write mode for the indicated cycle,
the process id is stored to be issued a wake up when the exiting process is
completed.

The cycle must be acquired prior to releasing the critical section as the
database could cycle between the time that crit is released and the ccp acts on
the message, as during that time, either the ccp or another process that enters
crit could initiate an WMEXIT.

This routine will not enter a request to exit write mode in the queue if the
critical section is owned as, having set the crit_blocked flag, it is
guaranteed that a GT.M process will send such a request.  This routine must set
the flag and then check crit, and REL_CRIT must release crit and then check the
flag in order to guarantee an exit write mode request being issued.

An exit write mode request must be issued from REL_CRIT because it can not be
guaranteed that another process will ever enter the critical section.

This scheme leaves a small hole open because of the possible interaction of a
second GT.M process.  With the use of only two boolean flags (in crit and crit
blocked) concurrency control cannot be guaranteed with the interaction of three
processes.  The hole is shown by the following sequence of events:

	1. GTM1 enters crit
	2. GTM1 releases crit
	3. GTM2 enters crit
	4. GTM2 checks ccp_crit_blocked, finds it clear and begins processing
	5. CCP sets crit blocked
	6. CCP checks in_crit, finds it non-zero and returns
	7. GTM1 checks crit_blocked, finds it set and issues an WMEXIT request
	8. CCP processes the WMEXIT request while GTM2 is processing in crit

To remove this hole, another flag or the use of a state flag and state
transistions rather than a boolean flag might be utilised.  Currently, it is
detected rather than prevented.  This is done in CCP_TR_EXITWM by checking when
the request has come from a GTM process to see if the critical section is
owned.  If it is and the process holding it is not the process that issued the
WMEXIT request, then a second process has slipped through the hole.  In this
case, the WMEXIT request is ignored, as the second process will issue a request
upon leaving the critical section.  The process id is stored so that upon
completion of the exiting process, the GTM process will be issued a wakeup.
Should the check be done after the second process has already left the critical
section, the CCP will continue exiting write mode.  It will receive a second
spurious request that will be ignored.  (And possibly even a third request from
a process entering the critical section which will also be ignored).

Two optimizations have been conceived to reduce the chances of a process
holding the critical section during the period that the database is not in read
mode.  The first of these is to have all GTM processes ensure that they are in
write mode before they attempt to grab the critical section.  The second is to
have GTM processes wait until the ccp_crit_blocked flag is clear before
attempting to grab the critical section.  This second optimization is not
currently implemented, because the use of CCP_USERWAIT requires that the
process receive a wakeup from the CCP.  To do this it must send a message and
the only appropriate message is a request for write mode.  Because the database
is currently in write mode, waiting to exit it, this results in a stream of
spurious messages being sent that clog the CCP's queues and mailbox.  To
implement this optimization will require a spin wait loop of some sort or the
introduction of a new message type allowing a GTM process to wait on the
clearing of the ccp_crit_blocked flag rather than on being in write mode.
If this optimization is implemented, the call to CCP_USERWAIT could be
eliminated from REL_CRIT.

***********************************************************************/
