/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_TRGFILE_PROTOS_INCLUDED
#define TRIGGER_TRGFILE_PROTOS_INCLUDED

STATICFNDCL boolean_t trigger_trgfile_tpwrap_helper(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt,
						    boolean_t lcl_implicit_tpwrap);

boolean_t trigger_trgfile_tpwrap(char *trigger_filename, uint4 trigger_filename_len, boolean_t noprompt);
#endif /* TRIGGER_TRGFILE_PROTOS_INCLUDED */
