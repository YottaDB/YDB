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

#ifndef TRIGGER_USER_NAME_H_INCLUDED
#define TRIGGER_USER_NAME_H_INCLUDED

boolean_t trigger_user_name(char *trigger_value, int trigger_value_len);
int validate_input_trigger_name(char *trigger_name, uint4 trigger_name_len, boolean_t *wildcard_ptr);
#endif
