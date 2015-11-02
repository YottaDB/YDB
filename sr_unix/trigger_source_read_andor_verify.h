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
#ifndef TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED
#define TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED

#ifdef GTM_TRIGGER
typedef enum
{
	TRIGGER_SRC_LOAD,
	TRIGGER_COMPILE
} trigger_action;

int trigger_source_read_andor_verify(mstr *trigname, trigger_action trigger_op);

#endif /* GTM_TRIGGER */

#endif /* TRIGGER_SOURCE_READ_ANDOR_VERIFY_H_INCLUDED */
