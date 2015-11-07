/****************************************************************
 *								*
 *	Copyright 2008, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note that each TAB_PROBECRIT_REC entry corresponds to a field in cs_addrs
 * Therefore, avoid any operation that changes the offset of any of these entries in the cs_addrs.
 * Additions are to be done at the END of the structures.
 * Replacing existing fields with new fields is allowed (provided their implications are thoroughly analyzed).
 */
TAB_PROBECRIT_REC(t_get_crit          , "CPT", "nanoseconds for the probe to get crit       ")
TAB_PROBECRIT_REC(p_crit_failed       , "CFN", "# of failures of the probe to get crit      ")
TAB_PROBECRIT_REC(p_crit_que_slps     , "CQN", "# of queue sleeps by the probe              ")
TAB_PROBECRIT_REC(p_crit_yields       , "CYN", "# of process yields by the probe            ")
TAB_PROBECRIT_REC(p_crit_que_full     , "CQF", "# of queue fulls encountered by the probe   ")
TAB_PROBECRIT_REC(p_crit_que_slots    , "CQE", "# of empty queue slots found by the probe   ")
TAB_PROBECRIT_REC(p_crit_success      , "CAT", "# of crit acquired total successes          ")
