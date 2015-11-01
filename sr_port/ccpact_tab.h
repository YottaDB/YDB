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
CCP_TABLE_ENTRY (transaction	routine		auxvalue  transaction
		  enum,		 name,		 type,	   priority )
*/
CCP_TABLE_ENTRY (CCTR_CHECKDB,	ccp_tr_checkdb,	 CCTVNUL, CCPR_NOR)	/* check files to see if still being accessed */
CCP_TABLE_ENTRY (CCTR_CLOSE,	ccp_tr_close,	 CCTVFIL, CCPR_NOR)	/* notify that process no longer has interest in region */
CCP_TABLE_ENTRY (CCTR_CLOSEJNL,	ccp_tr_closejnl, CCTVDBP, CCPR_HI)	/* journal file close due to oper ast */
CCP_TABLE_ENTRY (CCTR_DEBUG,	ccp_tr_debug,	 CCTVSTR, CCPR_HI)	/* request that ccp go into debug mode */
CCP_TABLE_ENTRY (CCTR_EWMWTBF,	ccp_tr_ewmwtbf,	 CCTVDBP, CCPR_HI)	/* exit write mode, wait for buffers to be written out */
CCP_TABLE_ENTRY (CCTR_EXITWM,	ccp_tr_exitwm,	 CCTVFIL, CCPR_NOR)	/* relinquish write mode */
CCP_TABLE_ENTRY (CCTR_EXWMREQ,	ccp_tr_exwmreq,	 CCTVDBP, CCPR_NOR)	/* request to relinquish write mode */
CCP_TABLE_ENTRY (CCTR_FLUSHLK,	ccp_tr_flushlk,	 CCTVFIL, CCPR_NOR)	/* wait for prior machine's flush to finish */
CCP_TABLE_ENTRY (CCTR_GOTDRT,	ccp_tr_gotdrt,	 CCTVDBP, CCPR_HI)	/* got enq for dirty buffers acquired */
CCP_TABLE_ENTRY (CCTR_LKRQWAKE,	ccp_tr_lkrqwake, CCTVFIL, CCPR_NOR)	/* request wake up lock wait procs whole cluster*/
CCP_TABLE_ENTRY (CCTR_NULL,	ccp_tr_null,	 CCTVNUL, CCPR_NOR)	/* noop */
CCP_TABLE_ENTRY (CCTR_OPENDB1,	ccp_tr_opendb1,	 CCTVFAB, CCPR_HI)	/* data base open, second phase */
CCP_TABLE_ENTRY (CCTR_OPENDB1A,	ccp_tr_opendb1a, CCTVFAB, CCPR_HI)	/* step two of first phase of opening */
CCP_TABLE_ENTRY (CCTR_OPENDB1B,	ccp_tr_opendb1b, CCTVDBP, CCPR_HI)	/* deadlock detected getting locks, retry */
CCP_TABLE_ENTRY (CCTR_OPENDB1E,	ccp_tr_opendb1e, CCTVFAB, CCPR_HI)	/* error opening database */
CCP_TABLE_ENTRY (CCTR_OPENDB3,	ccp_tr_opendb3,	 CCTVDBP, CCPR_HI)	/* data base open, third phase */
CCP_TABLE_ENTRY (CCTR_OPENDB3A,	ccp_tr_opendb3a, CCTVDBP, CCPR_HI)	/* journal file opening */
CCP_TABLE_ENTRY (CCTR_QUEDUMP,	ccp_tr_quedump,	 CCTVSTR, CCPR_HI)	/* request dump of queues */
CCP_TABLE_ENTRY (CCTR_STOP,	ccp_tr_stop,	 CCTVNUL, CCPR_NOR)	/* operator stop */
CCP_TABLE_ENTRY (CCTR_WRITEDB,	ccp_tr_writedb,	 CCTVFIL, CCPR_NOR)	/* request write mode */
CCP_TABLE_ENTRY (CCTR_WRITEDB1,	ccp_tr_writedb1, CCTVDBP, CCPR_HI)	/* request write mode - ENQ was granted */
