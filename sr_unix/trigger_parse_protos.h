/****************************************************************
 *								*
 * Copyright (c) 2010-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRIGGER_PARSE_PROTOS_INCLUDED
#define TRIGGER_PARSE_PROTOS_INCLUDED

boolean_t process_xecute(char *xecute_str, uint4 *xecute_len, boolean_t multi_line);

boolean_t check_trigger_name(char *name_str, uint4 *name_len);
boolean_t trigger_parse(char *input, uint4 input_len, char *trigvn, char **values, uint4 *value_len, int4 *max_len,
			boolean_t *multi_line_xecute);
#endif /* TRIGGER_PARSE_PROTOS_INCLUDED */
