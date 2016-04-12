/****************************************************************
 *								*
 * Copyright (c) 2011-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED
#define TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED

#ifdef GTM_TRIGGER

/* Header file shared by trigger_source_read_andor_verify() and trigger_locate_andor_load() since they
 * perform very similar function and share some macros and definitions.
 */

#define TRIG_FAILURE_RC	-1

#define	ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(TRIGNAME)								\
{														\
	DCL_THREADGBL_ACCESS;											\
														\
	SETUP_THREADGBL_ACCESS;											\
	if (!TREF(op_fntext_tlevel))										\
	{													\
		CLEAR_IMPLICIT_TP_BEFORE_ERROR;									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGNAMENF, 2, TRIGNAME->len, TRIGNAME->addr);	\
	}													\
}

/* If we have an implicit transaction and are about to fire an error, commit the transaction first so we can
 * get rid of the transaction connotation before error handling gets involved. Note we use op_tcommit() here
 * instead of op_trollback so we can verify the conditions that generated the error. If some restartable
 * condition caused the error, this will restart and retry the transaction. Note that since skip_INVOKE_RESTART
 * is not set before this op_tcommit, it with throw a restart rather than returning a restartable code.
 */
#define CLEAR_IMPLICIT_TP_BEFORE_ERROR							\
	if (dollar_tlevel && tp_pointer->implicit_trigger && (0 == gtm_trigger_depth))	\
	{	/* We have an implicit TP fence */					\
		enum cdb_sc		status;						\
		/* Eliminate transaction by commiting it (nothing was done) */		\
		status = op_tcommit();							\
		assert(cdb_sc_normal == status);					\
	}

gd_region *find_region(mstr *regname);
int trigger_locate_andor_load(mstr *trigname, rhdtyp **rtn_vec);
int trigger_source_read_andor_verify(mstr *trigname, rhdtyp **rtn_vec);

#endif /* GTM_TRIGGER */

#endif /* TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED */
