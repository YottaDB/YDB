/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef REPLGBL_H
#define REPLGBL_H

#define DEFAULT_JNL_RELEASE_TIMEOUT	300	    /* Default value for jnl_release_timeout is 5 minutes */

typedef struct
{
	boolean_t	trig_replic_warning_issued; /* TRUE if a warning to the source server log indicating a replicated triggered
						     * update to a non-trigger supporting secondary has been issued. Reset to FALSE
						     * at connection restart (gtmsource_recv_restart) */
	seq_num		trig_replic_suspect_seqno;  /* The sequence number at which the primary detected a triggered update being
						     * replicated. */
	boolean_t	null_subs_xform;	    /* TRUE if the null subscript collation is different between the servers */
	boolean_t	src_node_same_endianness;   /* TRUE if the source server has the same endianness as the receiver server */
	boolean_t	src_node_endianness_known;  /* TRUE if the endianness is already found out by the receiver server */
	boolean_t	srcsrv_vms;		    /* TRUE if the source server is VMS */
	int4		jnl_release_timeout;	    /* Timeout value for the jnl_release timer, in seconds */
} replgbl_t;

#endif
