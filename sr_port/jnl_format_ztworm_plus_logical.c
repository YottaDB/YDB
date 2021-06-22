/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF	mval		dollar_ztwormhole;
GBLREF	int4		gtm_trigger_depth;
GBLREF	int4		tstart_trigger_depth;
GBLREF	boolean_t	skip_dbtriggers;
GBLREF	boolean_t	explicit_update_repl_state;
GBLREF	uint4		dollar_tlevel;
GBLREF	jnl_gbls_t	jgbl;

/* This function formats a ZTWORMHOLE record (if applicable) followed by a logical journal record (SET/KILL/ZTRIGGER).
 * "ztworm_jfb", "jfb" and "jnl_format_done" are output parameters. The rest are input parameters.
 */
void jnl_format_ztworm_plus_logical(sgmnt_addrs *csa, boolean_t write_logical_jnlrecs, jnl_action_code jnl_op,
	gv_key *key, mval *val, jnl_format_buffer **ztworm_jfb, jnl_format_buffer **jfb, boolean_t *jnl_format_done)
{
	uint4			nodeflags;

	assert(dollar_tlevel);	/* tstart_trigger_depth is not usable otherwise */
	assert(tstart_trigger_depth <= gtm_trigger_depth);
	assert(!skip_dbtriggers); /* we ignore the JS_SKIP_TRIGGERS_MASK bit in nodeflags below because of this */
	if (tstart_trigger_depth == gtm_trigger_depth)
	{	/* explicit update so need to write ztwormhole records */
		assert(write_logical_jnlrecs == JNL_WRITE_LOGICAL_RECS(csa));
		nodeflags = 0;
		explicit_update_repl_state = REPL_ALLOWED(csa);
		/* Write ZTWORMHOLE records only if replicating since secondary is the only one that cares about it. */
		if (explicit_update_repl_state)
		{	/* Journal the $ZTWORMHOLE journal record BEFORE the corresponding SET record. If it is found
			 * that the trigger invocation did not REFERENCE/SET it, we will later remove this from the
			 * list of formatted journal records. Note that even if $ZTWORMHOLE is the empty string at this
			 * point (before the trigger invocation), we do this step. This is because we don't know if
			 * $ZTWORMHOLE can be modified to a different value inside the trigger code. If it is, we will
			 * need to add the $ZTWORMHOLE record then somewhere in the middle of the jnl format list. We
			 * could have implemented the code that way but that is non-trivial because journal recovery
			 * (mur_output_record.c) would need to then be fixed to process the ZTWORMHOLE journal record
			 * AFTER it has processed the immediately following SET/KILL/TRIG jnl record. Therefore, we
			 * stick to the current approach of adding a ZTWORM record in the jnl format list and later
			 * modifying it to reflect the post-trigger value (using JNL_ZTWORM_POST_TRIG opcode).
			 */
			*ztworm_jfb = jnl_format(JNL_ZTWORM, NULL, &dollar_ztwormhole, 0);
			assert(NULL != *ztworm_jfb);
		} else
			*ztworm_jfb = NULL;
	} else
	{	/* No need to write ZTWORMHOLE journal records for updates inside trigger since those records are not
		 * replicated anyway. Also update nodeflags accordingly.
		 */
		nodeflags = JS_NOT_REPLICATED_MASK;
		*ztworm_jfb = NULL;
	}
	assert(!*jnl_format_done);
	/* Need to write logical SET or KILL journal records irrespective of trigger depth */
	if (write_logical_jnlrecs)
	{
		nodeflags |= JS_HAS_TRIGGER_MASK;	/* gvt_trigger is non-NULL */
		/* Insert SET journal record now that ZTWORMHOLE (if any) has been inserted */
		*jfb = jnl_format(jnl_op, key, val, nodeflags);
		assert(NULL != *jfb);
		*jnl_format_done = TRUE;
	} else
		*jfb = NULL;
}

